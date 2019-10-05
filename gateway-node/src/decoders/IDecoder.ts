export interface IDecoder {
    decode(pulses: number[]): string | null;
    encode(code: string): number[] | null;
}
