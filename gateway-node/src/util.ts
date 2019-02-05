import { URL } from 'url';

import { EMPTY, NEVER, of } from 'rxjs';
import { ConnectableLayer } from './communication/message';
import { SerialLayer } from './communication/serial';
import { Telnet } from './communication/telnet';

export function getBaseLayer(address: string, logger: Logger): ConnectableLayer<Buffer> {
    const url = new URL(address);
    switch (url.protocol) {
        case 'serial:':
            const baudRate = parseInt(url.searchParams.get('baudRate'), 10) || undefined;
            return new SerialLayer(logger, url.pathname, baudRate);
        case 'telnet:':
            const port = parseInt(url.port, 10) || undefined;
            return new Telnet(logger, url.hostname, port);
        case 'debug:':
            return new DebugLayer();
        default:
            throw new Error(`protocol not supported: ${address}`);
    }
}


class DebugLayer implements ConnectableLayer<Buffer> {
    connected = of(true);

    data = NEVER;

    connect() {
    }

    close() {
    }

    send(arg: Buffer) {
        // tslint:disable-next-line:no-console
        console.log(arg.toString('hex'));
        return EMPTY;
    }
}
