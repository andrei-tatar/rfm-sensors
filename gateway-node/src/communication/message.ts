import { Observable } from 'rxjs/Observable';

export interface MessageLayer<T> {
    data: Observable<T>;
    send(arg: T): Observable<void>;
}
