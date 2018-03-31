export class SonyDecoder {
    private static readonly max_error = 15; // %

    private static readonly header_mark = 2400;
    private static readonly header_space = 600;

    private static readonly zero_mark = 600;
    private static readonly one_mark = 1200;

    private matches(value, target) {
        return Math.abs(value - target) / target * 100 < SonyDecoder.max_error;
    }

    decode(pulses: number[]) {
        if (pulses.length % 2 !== 1 || !this.matches(pulses[0], SonyDecoder.header_mark)) {
            return null;
        }

        let message = 0;
        let bits = 0;
        for (let i = 1; i < pulses.length; i += 2) {
            message *= 2;
            if (!this.matches(pulses[i], SonyDecoder.header_space)) { return null; }

            bits++;
            if (this.matches(pulses[i + 1], SonyDecoder.zero_mark)) {
                continue;
            }

            if (this.matches(pulses[i + 1], SonyDecoder.one_mark)) {
                message += 1;
                continue;
            }

            return null;
        }

        return `SONY_${message.toString(16)}_${bits}`;
    }

    encode(code) {
        if (typeof code !== 'string') {
            return null;
        }

        const parts = code.split('_');
        let bits;
        if (parts.length !== 3 || parts[0] !== 'SONY' || !isFinite(bits = parseInt(parts[2], 10))) {
            return null;
        }

        const nr = parseInt(parts[1], 16);
        if (isNaN(nr)) {
            return null;
        }

        const pulses = [SonyDecoder.header_mark, SonyDecoder.header_space];
        for (let i = bits - 1; i >= 0; i--) {
            pulses.push((nr & (1 << i)) !== 0 ? SonyDecoder.one_mark : SonyDecoder.zero_mark);
            if (i !== 0) { pulses.push(SonyDecoder.header_space); }
        }

        return pulses;
    }
}
