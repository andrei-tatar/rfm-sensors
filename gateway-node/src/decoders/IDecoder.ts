export interface IDecoder {
    decode(pulses: number[]): string;
    encode(code: string): number[];
}
