import { NEVER, Subject } from 'rxjs';
import { catchError, switchMap, takeUntil } from 'rxjs/operators';
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
            catchError(err => {
                this.error(err);
                return NEVER;
            }),
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
