import { writeFile } from 'fs';
import { cloneDeep, isEqual } from 'lodash';
import { homedir } from 'os';
import { join } from 'path';
import { BehaviorSubject, combineLatest, EMPTY, interval, Observable, Subject } from 'rxjs';
import {
    catchError, debounceTime, distinctUntilChanged, filter,
    map, publishReplay, refCount, scan, skip, startWith, switchMap, takeUntil, tap
} from 'rxjs/operators';
import { RadioNode } from '../communication/node';

interface HeatingSettings {
    enable: boolean;
    heaterAddress: number;
    histerezis: number;
    valveOpenPercent: number;
    valveClosePercent: number;
    rooms: {
        [roomName: string]: {
            on: boolean;
            setpoint: number;
            thermostatAddress: number;
            valveAddress: number;
        }
    };
}

interface ObservableNodes<T extends HeatingSettings> {
    settings$: Observable<HeatingSettings>;
    enable$: BehaviorSubject<boolean>;
    on$: BehaviorSubject<boolean>;
    heaterOn$: Observable<boolean>;
    histerezis: number;
    rooms: {
        [P in keyof T['rooms']]: RoomObservable;
    };
}

interface RoomObservable {
    on$: BehaviorSubject<boolean>;
    valveOpenPercent$: BehaviorSubject<number>;
    temperature$: Observable<number>;
    humidity$: Observable<number>;
    setpoint$: BehaviorSubject<number>;
    valveBattery$: Observable<number>;
    thermostatBattery$: Observable<number>;
}

const defaultSettings = {
    enable: false,
    heaterAddress: 9,
    histerezis: .5,
    valveOpenPercent: 80,
    valveClosePercent: 20,
    rooms: {
        'living': {
            on: false,
            setpoint: 22,
            thermostatAddress: 30,
            valveAddress: 4,
        },
        'office': {
            on: false,
            setpoint: 22,
            thermostatAddress: 31,
            valveAddress: 7,
        },
        'bedroom': {
            on: false,
            setpoint: 22,
            thermostatAddress: 32,
            valveAddress: 5,
        },
        'kids': {
            on: false,
            setpoint: 22,
            thermostatAddress: 33,
            valveAddress: 6,
        },
    },
};

module.exports = function (RED) {

    function MultiThermostatNode(config) {
        RED.nodes.createNode(this, config);

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) { return; }

        let settings = defaultSettings;

        const settingsFile = join(homedir(), 'thermostat-settings.json');

        const logger: Logger = RED.log;
        try {
            settings = require(settingsFile);
        } catch (err) {
            logger.info('no settings file, using default settings');
        }

        const close$ = new Subject();
        const connected: Observable<boolean> = bridge.connected;
        const nodes = createObservableNodes(this, settings, close$, bridge.create);

        combineLatest(connected, nodes.enable$, nodes.on$).pipe(
            takeUntil(close$)
        ).subscribe(([isConnected, enable, on]) => {
            this.status(isConnected
                ? { fill: 'green', shape: 'dot', text: `connected (E:${enable},O:${on})` }
                : { fill: 'red', shape: 'ring', text: 'not connected' });
        });

        nodes.settings$.pipe(
            skip(1),
            takeUntil(close$),
            map(s => JSON.stringify(s, null, 2)),
            switchMap(data => new Promise(resolve => {
                writeFile(settingsFile, data, err => {
                    if (err) { this.error(`while saving settings: ${err}`); }
                    resolve();
                });
            })),
        ).subscribe();

        const requireHeating: Observable<boolean>[] = [];
        for (const roomKey of Object.keys(nodes.rooms)) {
            const room: RoomObservable = nodes.rooms[roomKey];
            combineLatest(room.on$,
                room.setpoint$,
                room.humidity$.pipe(startWith(undefined)),
                room.temperature$.pipe(startWith(undefined)),
            ).pipe(debounceTime(0), takeUntil(close$))
                .subscribe(([on, setpoint, humidity, temperature]) => {
                    const payload: _.Dictionary<any> = {
                        mode: on ? 'heat' : 'off',
                        setpoint,
                    };
                    if (humidity !== void 0) { payload.humidity = humidity; }
                    if (temperature !== void 0) { payload.temperature = temperature; }
                    this.send({
                        topic: roomKey,
                        payload,
                    });
                });

            const requireHeating$ = combineLatest(room.temperature$, room.setpoint$, room.on$).pipe(
                debounceTime(0),
                scan<[number, number, boolean], boolean>((needsHeating, [temperature, setpoint, thermostatOn]) => {
                    if (!thermostatOn) { return false; }

                    const setpointLow = setpoint - settings.histerezis;
                    const setpointHigh = setpoint + settings.histerezis;
                    if (needsHeating) {
                        if (temperature >= setpointHigh) { return false; }
                    } else {
                        if (temperature <= setpointLow) { return true; }
                    }

                    return needsHeating;
                }, false),
                startWith(false),
                distinctUntilChanged(),
                tap(needsHeating =>
                    room.valveOpenPercent$.next(needsHeating
                        ? settings.valveOpenPercent
                        : settings.valveClosePercent
                    )
                ),
            );

            requireHeating.push(requireHeating$);
        }
        combineLatest(...requireHeating).pipe(
            map(needHeating => needHeating.some(r => r)),
            takeUntil(close$),
        ).subscribe(nodes.on$);

        nodes.enable$.pipe(takeUntil(close$))
            .subscribe(enable => this.send({
                topic: 'enable',
                payload: enable,
            }));

        nodes.heaterOn$.pipe(takeUntil(close$))
            .subscribe(on => this.send({
                topic: 'heater',
                payload: on,
            }));

        this.on('input', msg => {
            if (typeof msg !== 'object') { return; }

            if (msg.topic === 'enable') {
                nodes.enable$.next(!!msg.payload);
            }

            if (msg.topic in nodes.rooms) {
                const room: RoomObservable = nodes.rooms[msg.topic];
                if (typeof msg.payload.mode === 'string') {
                    room.on$.next(msg.payload.mode === 'on' || msg.payload.mode === 'heat');
                }
                if (typeof msg.payload.setpoint === 'number') {
                    room.setpoint$.next(msg.payload.setpoint);
                }
            }
        });

        this.on('close', () => {
            close$.next();
            close$.complete();
        });
    }

    RED.nodes.registerType('rfm-multithermostat', MultiThermostatNode);

    function createObservableNodes<T extends HeatingSettings>(
        this: unknown,
        node,
        settings: T,
        stop$: Observable<any>,
        create: (addr: number) => RadioNode,
    ): ObservableNodes<T> {

        const heater = create(settings.heaterAddress);
        const settingsSubject = new Subject<T>();
        const settings$ = settingsSubject.pipe(
            startWith(settings),
            debounceTime(0),
            distinctUntilChanged((a, b) => isEqual(a, b)),
            publishReplay(1),
            refCount(),
        );
        const on$ = new BehaviorSubject(false);
        const enable$ = new BehaviorSubject(settings.enable);
        const nodes: ObservableNodes<T> = {
            settings$,
            on$,
            enable$,
            histerezis: settings.histerezis,
            rooms: {} as any,
            heaterOn$: combineLatest(on$, enable$).pipe(
                debounceTime(0),
                map(([on, enable]) => on && enable),
                distinctUntilChanged(),
                publishReplay(1),
                refCount(),
            ),
        };

        nodes.enable$.pipe(skip(1), takeUntil(stop$)).subscribe(enable => {
            settings = cloneDeep(settings);
            settings.enable = enable;
            settingsSubject.next(settings);
        });

        combineLatest(nodes.heaterOn$, interval(60000).pipe(startWith(0))).pipe(
            switchMap(([on]) =>
                heater.send(Buffer.from([on ? 1 : 0])).pipe(catchError(err => {
                    node.error(`while updating heater state: ${err}`);
                    return EMPTY;
                }))
            ),
            takeUntil(stop$),
        ).subscribe();

        for (const key of Object.keys(settings.rooms)) {
            const roomSettings = settings.rooms[key];

            const thermostat = create(roomSettings.thermostatAddress);
            const values$ = thermostat.data.pipe(
                filter(msg => msg[0] === 0x01 && msg.length === 8),
                map(msg => ({
                    temperature: msg.readInt16BE(1) / 256, // deg C
                    humidity: msg.readInt16BE(3) / 100, // %
                    pressure: msg.readInt16BE(5) / 10, // hPa
                    battery: msg.readUInt8(7) / 100 + 1, // volts
                })),
                publishReplay(1),
                refCount(),
            );

            const thermostatBattery$ = values$.pipe(map(s => s.battery));
            const temperature$ = values$.pipe(map(s => s.temperature));
            const humidity$ = values$.pipe(map(s => s.humidity));

            const valve = create(roomSettings.valveAddress);
            const valveBattery$ = valve.data.pipe(filter(b => b.length === 2 && b[0] === 'B'.charCodeAt(0)), map(b => b[1] / 100 + 1));

            const room: RoomObservable = {
                on$: new BehaviorSubject(roomSettings.on),
                valveOpenPercent$: new BehaviorSubject(settings.valveClosePercent),
                temperature$,
                humidity$,
                setpoint$: new BehaviorSubject<number>(roomSettings.setpoint),
                valveBattery$,
                thermostatBattery$,
            };
            nodes.rooms[key] = room;

            const valvePoll$ = valve.data;
            valvePoll$.pipe(switchMap(_ => {
                let valvePercent = Math.round(room.valveOpenPercent$.value);
                valvePercent = Math.max(settings.valveClosePercent, Math.min(settings.valveOpenPercent, valvePercent));
                const temperature = room.on$.value ? Math.round(room.setpoint$.value * 10) : 0;
                return valve.send(Buffer.from([0xDE, valvePercent, temperature >> 8, temperature & 0xFF]))
                    .pipe(
                        catchError(err => {
                            node.error(`while updating ${key} valve: ${err}`);
                            return EMPTY;
                        })
                    );
            }), takeUntil(stop$)).subscribe();

            combineLatest(room.on$, room.setpoint$).pipe(debounceTime(0), takeUntil(stop$))
                .subscribe(([on, setpoint]) => {
                    settings = cloneDeep(settings);
                    settings.rooms[key].on = on;
                    settings.rooms[key].setpoint = setpoint;
                    settingsSubject.next(settings);
                });
        }

        return nodes;
    }
};
