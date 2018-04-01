import { BehaviorSubject } from 'rxjs/BehaviorSubject';
import { Observable } from 'rxjs/Observable';
import { Subject } from 'rxjs/Subject';

import { ConnectableLayer } from './message';

import * as net from 'net';

export class Telnet implements ConnectableLayer<Buffer> {
    private readonly _data = new Subject<Buffer>();
    private socket: net.Socket;
    private reconnectTimeout: NodeJS.Timer;
    private _connected = new BehaviorSubject(false);

    readonly data = this._data.asObservable();
    readonly connected = this._connected
        .distinctUntilChanged()
        .switchMap(c => {
            const fwd = Observable.of(c);
            if (c) {
                return Observable.timer(0, 5000)
                    .switchMap(s => this.sendRaw(Buffer.from([0xDE, 0x5B, 0x01, 0xFF, 0x40, 0x79]))) // heartbeat
                    .map(() => c)
                    .concat(fwd)
                    .catch(err => {
                        this.logger.warn(`could not send heartbeat ${err.message}`);
                        return fwd;
                    });
            }
            return fwd;
        })
        .distinctUntilChanged()
        .shareReplay(1);

    constructor(
        private logger: Logger,
        private host: string,
        private port: number = 23,
        private reconnectInterval: number = 5000,
    ) {
    }

    connect() {
        if (this.socket) {
            this.socket.destroy();
            this.socket = null;
        }

        this.socket = new net.Socket();
        this.socket.on('data', data => this._data.next(data));
        this.socket.once('disconnect', () => {
            this._connected.next(false);
            this.logger.warn('telnet: disconnected from server');
            this.reconnectTimeout = setTimeout(() => this.connect(), this.reconnectInterval);
        });
        this.socket.once('error', err => {
            this._connected.next(false);
            this.logger.warn(`telnet: error: ${err.message}`);
            this.reconnectTimeout = setTimeout(() => this.connect(), this.reconnectInterval);
        });

        this.logger.debug('telnet: connecting');
        this.socket.connect(this.port, this.host, async () => {
            this.logger.debug('telnet: connected');
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

    send(data: Buffer): Observable<void> {
        if (!this.socket) {
            return Observable.throw(new Error('telnet: not connected'));
        }

        return this._connected
            .first()
            .concatMap<boolean, void>(c => {
                if (!c) { throw new Error('telnet: not connected'); }
                return this.sendRaw(data);
            });
    }

    private sendRaw(data: Buffer) {
        return new Promise<void>(resolve => this.socket.write(data, resolve));
    }
}
