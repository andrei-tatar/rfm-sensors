import { EMPTY, NEVER, of } from 'rxjs';
import { ConnectableLayer } from './communication/message';

export class DebugLayer implements ConnectableLayer<Buffer> {
    connected = of(true);
    data = NEVER;
    send(arg: Buffer) {
        // tslint:disable-next-line:no-console
        console.log(arg.toString('hex'));
        return EMPTY;
    }
}
