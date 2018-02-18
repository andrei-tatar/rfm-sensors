import { Subject } from 'rxjs/Subject';
import { MessageLayer } from './message';

import 'rxjs/add/operator/scan';
import 'rxjs/add/operator/map';
import 'rxjs/add/operator/filter';

const FrameHeader1 = 0xDE, FrameHeader2 = 0x5B;

enum RxStatus {
    Idle, Header, Size, Data, Checksum1, Checksum2
}

interface State {
    status: RxStatus;
    buffer: Buffer;
    offset: number;
    chkSum: number;
    received: Buffer;
}

export class PackageLayer implements MessageLayer {
    get data() {
        return this.below.data
            .scan(
                (state, value) => {
                    if (state.received) {
                        state.received = null;
                    }
                    this.processReceivedData(state, value);
                    return state;
                }, {
                    status: RxStatus.Idle,
                    buffer: null,
                    offset: 0,
                    chkSum: 0,
                    received: null,
                } as State)
            .filter(v => {
                debugger;
                return v.received !== null;
            })
            .map(v => v.received);
    }

    constructor(private below: MessageLayer) {
    }

    private processReceivedData(state: State, rx: Buffer) {
        for (const data of rx) {
            switch (state.status) {
                case RxStatus.Idle:
                    if (data === FrameHeader1) state.status = RxStatus.Header;
                    break;
                case RxStatus.Header:
                    state.status = data === FrameHeader2 ? RxStatus.Size : RxStatus.Idle;
                    break;
                case RxStatus.Size:
                    state.buffer = new Buffer(data + 1);
                    state.offset = state.buffer.writeUInt8(data, 0);
                    state.status = RxStatus.Data;
                    break;
                case RxStatus.Data:
                    state.offset = state.buffer.writeUInt8(data, state.offset);
                    if (state.offset === state.buffer.length) state.status = RxStatus.Checksum1;
                    break;
                case RxStatus.Checksum1:
                    state.chkSum = data << 8;
                    state.status = RxStatus.Checksum2;
                    break;
                case RxStatus.Checksum2:
                    state.chkSum |= data;
                    const checksum = this.getChecksum(state.buffer, 0, state.buffer.length);
                    if (checksum === state.chkSum) {
                        const packet = new Buffer(state.buffer.length - 1);
                        state.buffer.copy(packet, 0, 1, state.buffer.length);
                        state.received = packet;
                    }
                    state.status = RxStatus.Idle;
                    break;

                default:
                    break;
            }
        }
    }

    private getChecksum(data: Buffer, offset: number, length: number) {
        let checksum = 0x1021;

        for (let i = 0; i < length; i++) {
            const byte = data[offset + i];
            const roll = (checksum & 0x8000) != 0 ? true : false;
            checksum <<= 1;
            checksum &= 0xFFFF;
            if (roll) checksum |= 1;
            checksum ^= byte;
        }

        return checksum;
    }

    async send(data: Buffer) {
        const packet = new Buffer(data.length + 5);
        let offset = packet.writeUInt8(FrameHeader1, 0);
        offset = packet.writeUInt8(FrameHeader2, offset);
        offset = packet.writeUInt8(data.length, offset);
        offset += data.copy(packet, offset, 0, data.length);
        const checksum = this.getChecksum(packet, 2, data.length + 1);
        packet.writeUInt16BE(checksum, offset);

        await this.below.send(packet);
    }
}