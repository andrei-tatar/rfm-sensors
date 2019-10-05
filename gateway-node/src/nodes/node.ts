export interface Node {
    on(ev: 'close', callback: (done: any) => void): void;
    on(ev: 'input', callback: (msg: any) => void): void;
    error(msg: string);
    status(options: { fill: string, shape: string, text: string }): void;
    send(msg: any): void;
}
