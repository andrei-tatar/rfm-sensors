import { Observable } from 'rxjs/Observable';
import { Subscription } from "rxjs/Subscription";
import { Subject } from 'rxjs/Subject';
import { BehaviorSubject } from 'rxjs/BehaviorSubject';
import { MessageLayer } from './message';

import * as net from 'net';

export class Telnet implements MessageLayer<Buffer> {
    private static readonly baudRate = 115200;

    private readonly _data: Subject<Buffer>;
    private socket: net.Socket;
    private reconnectTimeout: NodeJS.Timer;
    private _connected = new BehaviorSubject<boolean>(false);
    private hearbeat: Subscription;

    constructor(private host: string, private port: number = 23, private reconnectInterval: number = 5000) {
        this._data = new Subject<Buffer>();
    }

    get data() {
        return this._data.asObservable();
    }

    get connected() {
        return this._connected.asObservable().distinctUntilChanged();
    }

    open() {
        if (this.socket) {
            this.socket.destroy();
            this.socket = null;
        }

        this.socket = new net.Socket();
        this.socket.on('data', data => this._data.next(data));
        this.socket.once('disconnect', () => {
            this._connected.next(false);
            console.warn('telnet: disconnected from server');
            this.reconnectTimeout = setTimeout(() => this.open(), this.reconnectInterval);
        });
        this.socket.once('error', err => {
            this._connected.next(false);
            console.warn(`telnet: error: ${err.message}`);
            this.reconnectTimeout = setTimeout(() => this.open(), this.reconnectInterval);
        });

        console.debug('telnet: connecting');
        this.socket.connect(this.port, this.host, async () => {
            console.debug('telnet: connected');
            this._connected.next(true);
        });
    }

    close() {
        this._connected.next(false);
        this._connected.complete();
        this._data.complete();
        clearTimeout(this.reconnectTimeout);
        if (this.socket) {
            this.socket.destroy();
            this.socket = null;
        }
    }

    send(data: Buffer) {
        if (!this.socket) throw new Error('telnet: socket not initialized');

        return this._connected
            .first()
            .concatMap<boolean, void>(c => {
                if (!c) throw new Error('telnet: not connected');
                return new Promise(resolve => this.socket.write(data, resolve));
            });
    }
}
