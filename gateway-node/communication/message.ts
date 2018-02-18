import { Observable } from 'rxjs/Observable';

export interface MessageLayer {
    data: Observable<Buffer>;
    send(data: Buffer): Promise<void>;
}
