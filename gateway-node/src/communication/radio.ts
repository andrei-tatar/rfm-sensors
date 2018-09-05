import { Observable } from 'rxjs/Observable';
import { Subject } from 'rxjs/Subject';
import { MessageLayer } from './message';

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

    static readonly Err_InvalidSize = 0x71;
    static readonly Err_Busy = 0x72;
    static readonly Err_Addr = 0x73;
    static readonly Err_Mem = 0x74;
    static readonly Err_Timeout = 0x75;
}

export class RadioLayer implements MessageLayer<{ addr: number, data: Buffer }> {
    private _sendQueue = new Subject<SendMessage>();
    private _config: RadioConfig;

    readonly data = this.below
        .data
        .concatMap(p => {
            if (p[0] === Constants.Rsp_Init && this._config) {
                this.logger.warn('serial bridge reset; init again');
                return this.init(this._config)
                    .catch(err => {
                        this.logger.warn(`radio.reset-init: error: ${err.message}`);
                        return Observable.empty<Buffer>();
                    })
                    .switchMap(() => Observable.empty<Buffer>());
            }
            return Observable.of(p);
        })
        .filter(p => p[0] === Constants.Rsp_ReceivePacket)
        .map(p => {
            const msg = new Buffer(p.length - 2);
            p.copy(msg, 0, 2, p.length);
            return { data: msg, addr: p[1] };
        })
        .share();

    constructor(
        private below: MessageLayer<Buffer>,
        private logger: Logger,
    ) {
        this._sendQueue
            .concatMap(msg =>
                this.sendInternal(msg)
                    .do(() => { }, () => { }, () => msg.resolve())
                    .catch(err => {
                        msg.reject(err);
                        return Observable.empty();
                    })
            )
            .subscribe();
    }

    init({ key, freq, networkId, powerLevel }: RadioConfig = {}) {
        this._config = { key, freq, networkId, powerLevel };
        const aux = new Buffer(100);
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
            const configure = new Buffer(offset);
            aux.copy(configure, 0, 0, offset);
            this.logger.info('radio: sending configuration');
            return this.sendPacketAndWaitFor(configure, p => p.length === 2 && p[0] === Constants.Rsp_Configured);
        }
        return Observable.empty<void>();
    }

    send({ data, addr }: { addr: number, data: Buffer }) {
        return new Observable<void>(observer => {
            this._sendQueue.next({
                addr,
                data,
                resolve: () => observer.complete(),
                reject: err => observer.error(err),
            });
        });
    }

    private sendInternal({ data, addr }: SendMessage) {
        const pack = new Buffer(2 + data.length);
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

    private sendPacketAndWaitFor(packet: Buffer, verifyReply: (packet: Buffer) => boolean, timeout: number = 1000) {
        return this.below.data
            .filter(p => verifyReply(p))
            .first()
            .timeout(timeout)
            .merge(this.below.send(packet))
            .catch(err => {
                if (err.name === 'TimeoutError') {
                    throw new Error('timeout sending data');
                }
                throw err;
            });
    }
}

interface RadioConfig {
    key?: Buffer;
    freq?: number;
    networkId?: number;
    powerLevel?: number;
}
