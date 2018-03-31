import { Observable } from 'rxjs/Observable';

export interface MessageLayer<T = any> {
    data: Observable<T>;
    send(arg: T): Observable<void>;
}

export interface ConnectableLayer<T = any> extends MessageLayer<T> {
    readonly connected: Observable<boolean>;
    connect();
    close();
}
