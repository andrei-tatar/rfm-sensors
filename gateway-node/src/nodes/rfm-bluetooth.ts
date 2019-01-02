import { NEVER, Subject } from 'rxjs';
import { catchError, delay, retryWhen, switchMap, takeUntil, tap } from 'rxjs/operators';
import { BluetoothConnection } from '../communication/bluetooth';

module.exports = function (RED) {

    function BluetoothNode(config) {
        RED.nodes.createNode(this, config);

        if (!/^(?:[\dA-F]{2}:){5}[\dA-F]{2}$/i.test(config.address)) {
            return;
        }

        const device$ = BluetoothConnection.connect(config.address);
        const close$ = new Subject();

        device$.pipe(
            switchMap(d => d.status$),
            retryWhen(err => err.pipe(
                tap(error => this.error(error)),
                delay(10000),
            )),
            takeUntil(close$),
        ).subscribe(status => {
            this.send({ payload: { status } });
        });

        this.on('close', () => {
            close$.next();
            close$.complete();
        });
    }

    RED.nodes.registerType('rfm-bluetooth', BluetoothNode);
};
