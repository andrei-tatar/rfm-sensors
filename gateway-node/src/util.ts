import { URL, URLSearchParams } from 'url';

import { ConnectableLayer } from './communication/message';
import { PackageLayer } from './communication/package';
import { RadioConfig, RadioLayer } from './communication/radio';
import { SerialLayer } from './communication/serial';
import { Telnet } from './communication/telnet';
import { DebugLayer } from './DebugLayer';
import { Logger } from './Logger';

function getBaseLayer(address: string, logger: Logger): ConnectableLayer<Buffer> {
    const url = new URL(address);
    switch (url.protocol) {
        case 'serial:':
            const baudRate = readParamAsNumber(url.searchParams, 'baudRate');
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

export function getRadioLayer(address: string, logger: Logger, config: Partial<RadioConfig> = {}): RadioLayer {
    const url = new URL(address);
    const base = getBaseLayer(address, logger);
    const packageLayer = new PackageLayer(base);
    const key = url.searchParams.get('key') || undefined;
    const requireHeartbeatEcho = url.searchParams.get('hb') !== null;
    const radioLayer = new RadioLayer(packageLayer, logger, {
        ...config,
        key,
        powerLevel: readParamAsNumber(url.searchParams, 'power'),
        freq: readParamAsNumber(url.searchParams, 'freq'),
        networkId: readParamAsNumber(url.searchParams, 'id'),
        requireHeartbeatEcho,
    });
    return radioLayer;
}

function readParamAsNumber(params: URLSearchParams, name: string): number | undefined {
    const value = params.get(name);
    if (value === null) { return undefined; }
    const numberValue = parseFloat(value);
    if (!isFinite(numberValue) || isNaN(numberValue)) { return undefined; }
    return numberValue;
}
