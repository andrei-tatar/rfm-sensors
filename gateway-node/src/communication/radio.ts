import { MessageLayer } from './message';
import { Observable } from 'rxjs/Observable';
import { Subject } from 'rxjs/Subject';

import 'rxjs/add/operator/groupBy'
import 'rxjs/add/operator/concatMap'
import 'rxjs/add/operator/mergeMap'
import 'rxjs/add/operator/timeout'
import 'rxjs/add/operator/catch'

interface SendMessage {
    to: number, data: Buffer, resolve: Function, reject: Function;
}

export class RadioLayer {
    private _sendQueue = new Subject<SendMessage>();

    constructor(
        private below: MessageLayer,
    ) {
        this._sendQueue
            .groupBy(p => p.to)
            .mergeMap(group => group.concatMap(async msg => this.sendInternal(msg)))
            .subscribe();
    }

    private async sendInternal(msg: SendMessage) {
        try {
            const pack = new Buffer(2 + msg.data.length);
            pack[0] = Constants.Cmd_SendPacket; // FRAME_SENDPACKET
            pack[1] = msg.to;
            msg.data.copy(pack, 2);

            await this.sendPacketAndWaitFor(pack,
                r => {
                    if (r[1] !== msg.to) return;
                    switch (r[0]) {
                        case Constants.Rsp_PacketSent: return true;

                        case Constants.Err_Addr: throw new Error(`invalid address ${r[1]}`);
                        case Constants.Err_Busy: throw new Error(`address already busy ${r[1]}`);
                        case Constants.Err_InvalidSize: throw new Error(`invalid size ${msg.data.length}`);
                        case Constants.Err_Mem: throw new Error(`gw memory full`);
                        case Constants.Err_Timeout: throw new Error(`timeout. no ack (${msg.to})`);
                    }
                });
            msg.resolve(true);
        } catch (err) {
            msg.reject(err);
        }
    }

    private async sendPacketAndWaitFor(packet: Buffer, verifyReply: (packet: Buffer) => boolean, timeout: number = 600) {
        const waiter = this.below.data
            .filter(p => verifyReply(p))
            .first()
            .timeout(timeout)
            .toPromise();
        await this.below.send(packet);
        await waiter;
    }

    async init({ key, freq, networkId, powerLevel }: RadioConfig = {}) {
        const aux = new Buffer(100);
        let offset = aux.writeUInt8(Constants.Cmd_Configure, 0);
        if (key !== void 0) {
            if (key.length !== 16) throw new Error(`Invalid AES-128 key size (${key.length})`);
            offset = aux.writeUInt8('K'.charCodeAt(0), offset);
            offset += key.copy(aux, offset);
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
            await this.sendPacketAndWaitFor(configure, p => p.length == 2 && p[0] == Constants.Rsp_Configured);
        }
    }

    get data(): Observable<{ from: number, data: Buffer }> {
        return this.below
            .data
            .filter(p => p[0] === Constants.Rsp_ReceivePacket)
            .map(p => {
                const msg = new Buffer(p.length - 2);
                p.copy(msg, 0, 2, p.length);
                return { data: msg, from: p[1] };
            });
    }

    send(to: number, data: Buffer) {
        if (data.length > 56) return Promise.reject(new Error(`invalid msg size ${data.length}`));
        return new Promise((resolve, reject) => this._sendQueue.next({ to, data, resolve, reject }));
    }
}

class Constants {
    static readonly Cmd_Configure = 0x90;
    static readonly Rsp_Configured = 0x91;

    static readonly Cmd_SendPacket = 0x92;
    static readonly Rsp_PacketSent = 0x93;
    static readonly Rsp_ReceivePacket = 0x94;

    static readonly Err_InvalidSize = 0x71;
    static readonly Err_Busy = 0x72;
    static readonly Err_Addr = 0x73;
    static readonly Err_Mem = 0x74;
    static readonly Err_Timeout = 0x75;
}

interface RadioConfig {
    key?: Buffer;
    freq?: number;
    networkId?: number;
    powerLevel?: number;
}
