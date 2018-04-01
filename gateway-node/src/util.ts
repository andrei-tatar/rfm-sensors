import { URL } from 'url';

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
        default:
            throw new Error(`protocol not supported: ${address}`);
    }
}
