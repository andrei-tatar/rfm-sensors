import * as dbus from 'dbus-native';
import { defer, interval, Observable } from 'rxjs';
import { distinctUntilChanged, map, publishReplay, refCount, switchMap } from 'rxjs/operators';

export class BluetoothMediaDevice {

    readonly status$ = interval(1000).pipe(
        switchMap(_ => Util.readProperty<string>(this.iface, 'Status')),
        distinctUntilChanged(),
        publishReplay(1),
        refCount(),
    );

    constructor(
        private iface,
    ) {

    }
}

export class Util {
    static getInterface(service, path: string, interfaceName: string) {
        return defer(() => new Promise((resolve, reject) => {
            service.getInterface(path, interfaceName, (err, instance) => {
                if (err) { reject(err); } else { resolve(instance); }
            });
        }));
    }

    static readProperty<T = any>(ifInstance, property: string) {
        return defer(() => new Promise<T>((resolve, reject) => {
            ifInstance.$readProp(property, (err, value) => {
                if (err) { reject(err); } else { resolve(value); }
            });
        }));
    }
}

export class BluetoothConnection {
    static connect(mac: string) {
        mac = mac.replace(/:/g, '_');
        return new Observable<any>(observer => {
            const sessionBus = dbus.systemBus();
            observer.next(sessionBus);
            return () => sessionBus.connection.end();
        }).pipe(
            map(bus => bus.getService('org.bluez')),
            switchMap(service => Util.getInterface(service, `/org/bluez/hci0/dev_${mac}`, 'org.bluez.MediaControl1').pipe(
                switchMap(inst => Util.readProperty<string>(inst, 'Player')),
                switchMap(player => Util.getInterface(service, player, 'org.bluez.MediaPlayer1'))
            )),
            map(iface => new BluetoothMediaDevice(iface)),
            publishReplay(1),
            refCount(),
        );
    }
}
