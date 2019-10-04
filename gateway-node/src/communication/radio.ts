import { concat, EMPTY, interval, merge, Observable, of, Subject } from 'rxjs';
import {
    catchError, concatMap, delay, distinctUntilChanged,
    filter, first, ignoreElements, map, publishReplay,
    refCount, retryWhen, share, switchMap, tap, timeout
} from 'rxjs/operators';

import { Logger } from '../Logger';
import { ConnectableLayer } from './message';

interface SendMessage {
    addr: number;
    data: Buffer;
    resolve: Function;
    reject: Function;
}

class Constants {
    static readonly Cmd_Configure = 0x90;
    static readonly Rsp_Configured = 0x91;

    static readonly Cmd_SendPacket = 0x92;
    static readonly Rsp_PacketSent = 0x93;
    static readonly Rsp_ReceivePacket = 0x94;

    static readonly Rsp_Init = 0x95;
    static readonly HearBeat = 0x96;

    static readonly Err_InvalidSize = 0x71;
    static readonly Err_Busy = 0x72;
    static readonly Err_Addr = 0x73;
    static readonly Err_Mem = 0x74;
    static readonly Err_Timeout = 0x75;
}

export class RadioLayer implements ConnectableLayer<{ addr: number, data: Buffer }> {
    private _sendQueue = new Subject<SendMessage>();
    private _config: RadioConfig;

    readonly data = this.below
        .data
        .pipe(
            concatMap(p => {
                if (p[0] === Constants.Rsp_Init && this._config) {
                    this.logger.warn('serial bridge reset; init again');
                    return this.reinit();
                }
                return of(p);
            }),
            filter(p => p[0] === Constants.Rsp_ReceivePacket),
            map(p => {
                const msg = Buffer.alloc(p.length - 2);
                p.copy(msg, 0, 2, p.length);
                return { data: msg, addr: p[1] };
            }),
            share(),
        );

    readonly connected: Observable<boolean>;

    constructor(
        private below: ConnectableLayer<Buffer>,
        private logger: Logger,
        {
            key,
            power = 31,
            heartBeatInterval = 60e3,
            requireHeartbeatEcho = false,
        }: {
            key: string, power?: number,
            heartBeatInterval?: number,
            requireHeartbeatEcho?: boolean,
        },
    ) {
        this._sendQueue
            .pipe(
                concatMap(msg =>
                    this.sendInternal(msg)
                        .pipe(
                            tap(() => { }, () => { }, () => msg.resolve()),
                            catchError(err => {
                                msg.reject(err);
                                return EMPTY;
                            })
                        )
                )
            )
            .subscribe();
        this.connected = this.below.connected
            .pipe(
                switchMap(isConnected => {
                    if (isConnected) {
                        const init$ = this
                            .init({
                                key: key && Buffer.from(key, 'hex'),
                                powerLevel: power,
                            });
                        const heartBeat$ = !requireHeartbeatEcho
                            ? EMPTY
                            : interval(heartBeatInterval).pipe(
                                switchMap(() =>
                                    this.sendPacketAndWaitFor(
                                        Buffer.from([Constants.HearBeat]),
                                        p => p.length === 2 && p[0] === Constants.HearBeat
                                    ).pipe(
                                        catchError(() => {
                                            throw new Error('did not receive heartbeat');
                                        })
                                    )
                                ),
                            );
                        return concat(init$, of(isConnected), heartBeat$);
                    }
                    return of(isConnected);
                }),
                retryWhen(errs => errs.pipe(
                    delay(5000),
                )),
                distinctUntilChanged(),
                publishReplay(1),
                refCount(),
            );
    }

    private init({ key, freq, networkId, powerLevel }: RadioConfig = {}) {
        this._config = { key, freq, networkId, powerLevel };
        const aux = Buffer.alloc(100);
        let offset = aux.writeUInt8(Constants.Cmd_Configure, 0);
        if (key !== void 0) {
            if (key.length !== 16) { throw new Error(`Invalid AES-128 key size (${key.length})`); }
            offset = aux.writeUInt8('K'.charCodeAt(0), offset);
            offset += key.copy(aux, offset);

            offset = aux.writeUInt8('R'.charCodeAt(0), offset);
            offset += aux.writeUInt16LE(Math.floor(Math.random() * 65536), offset);
        }
        if (freq !== void 0) {
            offset = aux.writeUInt8('F'.charCodeAt(0), offset);
            offset = aux.writeUInt32LE(freq, offset);
        }
        if (networkId !== void 0) {
            offset = aux.writeUInt8('N'.charCodeAt(0), offset);
            offset = aux.writeUInt8(networkId, offset);
        }
        if (powerLevel !== void 0) {
            offset = aux.writeUInt8('P'.charCodeAt(0), offset);
            offset = aux.writeUInt8(powerLevel, offset);
        }
        if (offset !== 0) {
            const configure = Buffer.alloc(offset);
            aux.copy(configure, 0, 0, offset);
            this.logger.info('radio: sending configuration');
            return this.sendPacketAndWaitFor(configure, p => p.length === 2 && p[0] === Constants.Rsp_Configured)
                .pipe(tap(() => { }, () => { }, () => this.logger.info('radio: configuration sent!')));
        }
        return EMPTY;
    }

    send({ data, addr }: { addr: number, data: Buffer }) {
        return new Observable<never>(observer => {
            this._sendQueue.next({
                addr,
                data,
                resolve: () => observer.complete(),
                reject: err => observer.error(err),
            });
        });
    }

    private reinit() {
        return this.init(this._config)
            .pipe(
                ignoreElements(),
                catchError(err => {
                    this.logger.warn(`radio.reset-init: error: ${err.message}`);
                    return EMPTY;
                }),
            );
    }

    private sendInternal({ data, addr }: SendMessage) {
        const pack = Buffer.alloc(2 + data.length);
        pack[0] = Constants.Cmd_SendPacket; // FRAME_SENDPACKET
        pack[1] = addr;
        data.copy(pack, 2);

        return this.sendPacketAndWaitFor(pack,
            r => {
                if (r[1] !== addr) { return false; }
                switch (r[0]) {
                    case Constants.Rsp_PacketSent: return true;

                    case Constants.Err_Addr: throw new Error(`invalid address ${r[1]}`);
                    case Constants.Err_Busy: throw new Error(`address already busy ${r[1]}`);
                    case Constants.Err_InvalidSize: throw new Error(`invalid size ${data.length}`);
                    case Constants.Err_Mem: throw new Error(`gw memory full`);
                    case Constants.Err_Timeout: throw new Error(`timeout. no ack (${addr})`);
                }
            });
    }

    private sendPacketAndWaitFor(packet: Buffer, verifyReply: (packet: Buffer) => boolean, timeoutTime: number = 1200) {
        const waitForReply$ = this.below.data.pipe(
            filter(p => verifyReply(p)),
            first(),
            timeout(timeoutTime)
        );
        return merge(waitForReply$, this.below.send(packet))
            .pipe(
                catchError(err => {
                    if (err.name === 'TimeoutError') {
                        throw new Error('timeout sending data');
                    }
                    throw err;
                }),
                ignoreElements(),
            );
    }
}

interface RadioConfig {
    key?: Buffer;
    freq?: number;
    networkId?: number;
    powerLevel?: number;
}
