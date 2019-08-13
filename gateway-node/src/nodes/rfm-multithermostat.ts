import { writeFile } from 'fs';
import { cloneDeep, isEqual } from 'lodash';
import { homedir } from 'os';
import { join } from 'path';
import { BehaviorSubject, combineLatest, EMPTY, interval, Observable, Subject } from 'rxjs';
import {
    catchError, debounceTime, distinctUntilChanged, filter, map,
    publishReplay, refCount, scan, skip, startWith, switchMap, takeUntil
} from 'rxjs/operators';
import { RadioNode } from '../communication/node';

type ThermostatMode = 'off' | 'heat' | 'cool';

interface HeatingSettings {
    enable: boolean;
    heaterAddress: number;
    histerezis: number;
    valveOpenPercent: number;
    valveClosePercent: number;
    rooms: {
        [roomName: string]: {
            mode: ThermostatMode;
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
    mode$: BehaviorSubject<ThermostatMode>;
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
    histerezis: .3,
    valveOpenPercent: 0,
    valveClosePercent: 85,
    rooms: {
        'living': {
            mode: 'off' as ThermostatMode,
            setpoint: 22,
            thermostatAddress: 30,
            valveAddress: 4,
        },
        'office': {
            mode: 'off' as ThermostatMode,
            setpoint: 22,
            thermostatAddress: 31,
            valveAddress: 7,
        },
        'bedroom': {
            mode: 'off' as ThermostatMode,
            setpoint: 22,
            thermostatAddress: 32,
            valveAddress: 5,
        },
        'kids': {
            mode: 'off' as ThermostatMode,
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

        let useSettings = defaultSettings;

        const settingsFile = join(homedir(), 'thermostat-settings.json');

        const logger: Logger = RED.log;
        try {
            useSettings = require(settingsFile);
        } catch (err) {
            logger.info('no settings file, using default settings');
        }

        const close$ = new Subject();
        const connected: Observable<boolean> = bridge.connected;
        const createNodeObservables: typeof createObservableNodes = createObservableNodes.bind(this);
        const nodes$ = createNodeObservables(useSettings, bridge.create);

        combineLatest(connected, nodes$.enable$, nodes$.on$).pipe(
            takeUntil(close$)
        ).subscribe(([isConnected, enable, on]) => {
            this.status(isConnected
                ? { fill: 'green', shape: 'dot', text: `connected (E:${enable},O:${on})` }
                : { fill: 'red', shape: 'ring', text: 'not connected' });
        });

        nodes$.settings$.pipe(
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

        const sendNodeChange: typeof sendChange = sendChange.bind(this);

        const requireHeating: Observable<boolean>[] = [];
        for (const roomKey of Object.keys(nodes$.rooms)) {
            const room: RoomObservable = nodes$.rooms[roomKey];

            sendNodeChange(room.mode$, roomKey, mode => ({ mode }));
            sendNodeChange(room.setpoint$, roomKey, setpoint => ({ setpoint }));
            sendNodeChange(combineLatest(room.temperature$, room.humidity$), roomKey,
                ([temperature, humidity]) => ({ temperature, humidity })
            );
            sendNodeChange(room.thermostatBattery$, `battery:thermostat-${roomKey}`);
            sendNodeChange(room.valveBattery$, `battery:valve-${roomKey}`);

            const requireHeating$ = combineLatest(room.temperature$, room.setpoint$, room.mode$).pipe(
                debounceTime(0),
                scan<[number, number, ThermostatMode], boolean>((needsHeating, [temperature, setpoint, mode]) => {
                    if (mode !== 'heat') { return false; }

                    const setpointLow = setpoint - useSettings.histerezis;
                    const setpointHigh = setpoint + useSettings.histerezis;
                    if (needsHeating) {
                        if (temperature >= setpointHigh) { return false; }
                    } else {
                        if (temperature <= setpointLow) { return true; }
                    }

                    return needsHeating;
                }, false),
                startWith(false),
                distinctUntilChanged(),
                publishReplay(1),
                refCount(),
            );

            combineLatest(requireHeating$, nodes$.heaterOn$, nodes$.enable$).pipe(
                debounceTime(500),
                takeUntil(close$),
            ).subscribe(([needsHeating, heaterOn, heaterEnabled]) => {
                if (!heaterEnabled) {
                    room.valveOpenPercent$.next(useSettings.valveOpenPercent);
                } else if (heaterOn) {
                    room.valveOpenPercent$.next(needsHeating
                        ? useSettings.valveOpenPercent
                        : useSettings.valveClosePercent
                    );
                }
            });

            requireHeating.push(requireHeating$);
        }
        combineLatest(...requireHeating).pipe(
            map(needHeating => needHeating.some(r => r)),
            takeUntil(close$),
        ).subscribe(nodes$.on$);

        sendNodeChange(nodes$.enable$, 'enable');
        sendNodeChange(nodes$.heaterOn$, 'heater');

        this.on('input', msg => {
            if (typeof msg !== 'object') { return; }

            if (msg.topic === 'enable') {
                nodes$.enable$.next(!!msg.payload);
            }

            if (msg.topic in nodes$.rooms) {
                const room: RoomObservable = nodes$.rooms[msg.topic];
                if (typeof msg.payload.mode === 'string' && ['heat', 'cool', 'off'].includes(msg.payload.mode)) {
                    room.mode$.next(msg.payload.mode);
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

        function sendChange<T>(observable: Observable<T>, topic: string, getPayload: (value: T) => any = v => v) {
            observable.pipe(
                debounceTime(0),
                takeUntil(close$),
            ).subscribe(value => this.send({
                topic,
                payload: getPayload(value),
            }));
        }

        function createObservableNodes<T extends HeatingSettings>(
            settings: T,
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

            nodes.enable$.pipe(skip(1), takeUntil(close$)).subscribe(enable => {
                settings = cloneDeep(settings);
                settings.enable = enable;
                settingsSubject.next(settings);
            });

            combineLatest(
                nodes.heaterOn$.pipe(distinctUntilChanged(), debounceTime(5000)),
                interval(60000).pipe(startWith(0))
            ).pipe(
                switchMap(([on]) =>
                    heater.send(Buffer.from([on ? 1 : 0])).pipe(catchError(err => {
                        this.error(`while updating heater state: ${err}`);
                        return EMPTY;
                    }))
                ),
                takeUntil(close$),
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
                    mode$: new BehaviorSubject(roomSettings.mode || 'off'),
                    valveOpenPercent$: new BehaviorSubject(settings.valveOpenPercent),
                    temperature$,
                    humidity$,
                    setpoint$: new BehaviorSubject<number>(roomSettings.setpoint),
                    valveBattery$,
                    thermostatBattery$,
                };
                nodes.rooms[key as keyof T['rooms']] = room;

                const valvePoll$ = valve.data;
                valvePoll$.pipe(switchMap(_ => {
                    const valvePercent = Math.max(0, Math.min(100, Math.round(room.valveOpenPercent$.value)));
                    const temperature = room.mode$.value === 'heat' ? Math.round(room.setpoint$.value * 10) : 0;
                    return valve.send(Buffer.from([0xDE, valvePercent, temperature >> 8, temperature & 0xFF]))
                        .pipe(
                            catchError(err => {
                                this.error(`while updating ${key} valve: ${err}`);
                                return EMPTY;
                            })
                        );
                }), takeUntil(close$)).subscribe();

                const thermostatPoll$ = thermostat.data.pipe(filter(m => m.length === 1 && m[0] === 2));
                thermostatPoll$.pipe(switchMap(_ => {
                    const response = Buffer.alloc(3);
                    const setpoint = Math.max(0, Math.min(255, Math.round(room.setpoint$.value * 10) - 100));
                    let offset = response.writeUInt8(2, 0);
                    offset = response.writeUInt8(room.mode$.value === 'heat' ? 1 : 0, offset);
                    offset = response.writeUInt8(setpoint, offset);
                    return thermostat.send(response).pipe(
                        catchError(err => {
                            this.error(`while responding to thermostat ${key}: ${err}`);
                            return EMPTY;
                        })
                    );
                }), takeUntil(close$)).subscribe();

                const thermostatCommand$ = thermostat.data.pipe(filter(m => m.length === 3 && m[0] === 3));
                thermostatCommand$.pipe(takeUntil(close$)).subscribe(cmd => {
                    const on = (cmd.readUInt8(1) & 1) === 1;
                    if (on) {
                        const setpoint = cmd.readUInt8(2);
                        room.mode$.next('heat');
                        room.setpoint$.next((setpoint + 100) / 10);
                    }
                });

                combineLatest(room.mode$, room.setpoint$).pipe(
                    debounceTime(0),
                    takeUntil(close$),
                ).subscribe(([mode, setpoint]) => {
                    settings = cloneDeep(settings);
                    settings.rooms[key].mode = mode;
                    settings.rooms[key].setpoint = setpoint;
                    settingsSubject.next(settings);
                });
            }

            return nodes;
        }
    }

    RED.nodes.registerType('rfm-multithermostat', MultiThermostatNode);
};
