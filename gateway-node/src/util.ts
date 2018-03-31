import { URL } from 'url';

import { SerialLayer } from './communication/serial';
import { Telnet } from './communication/telnet';

export function getBaseLayer(address: string, logger: Logger) {
    const url = new URL(address);
    switch (url.protocol) {
        case 'serial:':
            const baudRate = parseInt(url.searchParams.get('baudRate'), 10) || undefined;
            return new SerialLayer(logger, url.host, baudRate);
        case 'telnet:':
            const port = parseInt(url.port, 10) || undefined;
            return new Telnet(logger, url.hostname, port);
        default:
            throw new Error(`protocol not supported: ${address}`);
    }
}

getBaseLayer('telnet://test', console);
