export class SamsungDecoder {
    private static readonly max_error = 15; // %

    private static readonly header_mark = 4500;
    private static readonly header_space = 4500;

    private static readonly bit_mark = 590;
    private static readonly zero_space = 590;
    private static readonly one_space = 1690;

    private static readonly stop_mark = 590;

    private matches(value, target) {
        return Math.abs(value - target) / target * 100 < SamsungDecoder.max_error;
    }

    decode(pulses) {
        if (pulses.length !== 67 || !this.matches(pulses[0], SamsungDecoder.header_mark) ||
            !this.matches(pulses[1], SamsungDecoder.header_space)) {
            return null;
        }

        let message = 0;
        for (let i = 0; i < 32; i++) {
            message *= 2;

            if (!this.matches(pulses[2 + i * 2], SamsungDecoder.bit_mark)) {
                return null;
            }

            if (this.matches(pulses[3 + i * 2], SamsungDecoder.zero_space)) {
                continue;
            }

            if (this.matches(pulses[3 + i * 2], SamsungDecoder.one_space)) {
                message += 1;
                continue;
            }

            return null;
        }

        if (!this.matches(pulses[66], SamsungDecoder.stop_mark)) {
            return null;
        }

        return 'SAM_' + message.toString(16);
    }

    encode(code) {
        if (typeof code !== 'string') {
            return null;
        }

        const parts = code.split('_');
        if (parts.length !== 2 || parts[0] !== 'SAM') {
            return null;
        }

        const nr = parseInt(parts[1], 16);
        if (isNaN(nr)) {
            return null;
        }

        const pulses = [SamsungDecoder.header_mark, SamsungDecoder.header_space];
        for (let i = 31; i >= 0; i--) {
            pulses.push(SamsungDecoder.bit_mark);
            pulses.push((nr & (1 << i)) !== 0 ? SamsungDecoder.one_space : SamsungDecoder.zero_space);
        }
        pulses.push(SamsungDecoder.bit_mark);

        return pulses;
    }
}
