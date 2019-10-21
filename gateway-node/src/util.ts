import { URL } from 'url';

import { ConnectableLayer } from './communication/message';
import { PackageLayer } from './communication/package';
import { RadioLayer } from './communication/radio';
import { SerialLayer } from './communication/serial';
import { Telnet } from './communication/telnet';
import { DebugLayer } from './DebugLayer';
import { Logger } from './Logger';

function getBaseLayer(address: string, logger: Logger): ConnectableLayer<Buffer> {
    const url = new URL(address);
    switch (url.protocol) {
        case 'serial:':
            const baudParam = url.searchParams.get('baudRate');
            const baudRate = (baudParam != null ? parseInt(baudParam, 10) || undefined : undefined);
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

export function getPackageLayer(address: string, logger: Logger): PackageLayer {
    const base = getBaseLayer(address, logger);
    const pckg = new PackageLayer(base, true);
    return pckg;
}

export function getRadioLayer(address: string, logger: Logger): RadioLayer {
    const url = new URL(address);
    const base = getBaseLayer(address, logger);
    const packageLayer = new PackageLayer(base);
    const key = url.searchParams.get('key') || undefined;
    const powerParam = url.searchParams.get('power');
    const powerLevel = powerParam !== null ? (parseInt(powerParam, 10) || undefined) : undefined;
    const requireHeartbeatEcho = url.searchParams.get('hb') !== null;
    const freqParam = url.searchParams.get('freq');
    const freq = freqParam ? (parseFloat(freqParam) || undefined) : undefined;
    const radioLayer = new RadioLayer(packageLayer, logger, {
        key,
        powerLevel,
        requireHeartbeatEcho,
        freq,
    });
    return radioLayer;
}
