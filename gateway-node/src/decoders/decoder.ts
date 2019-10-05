import { IDecoder } from './IDecoder';
import { NecDecoder } from './nec';
import { SamsungDecoder } from './samsung';
import { SonyDecoder } from './sony';

export class Decoder implements IDecoder {

    private decoders: IDecoder[] = [];

    constructor() {
        this.decoders.push(new SonyDecoder());
        this.decoders.push(new NecDecoder());
        this.decoders.push(new SamsungDecoder());
    }

    decode(pulses: number[]) {
        for (const decoder of this.decoders) {
            const code = decoder.decode(pulses);
            if (code) { return code; }
        }
        return null;
    }

    encode(code: string) {
        for (const decoder of this.decoders) {
            const pulses = decoder.encode(code);
            if (pulses) { return pulses; }
        }
        return null;
    }
}
