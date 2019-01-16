import { Subject } from 'rxjs';

module.exports = function (RED) {

    function MultiThermostatNode(config) {
        RED.nodes.createNode(this, config);

        const bridge = RED.nodes.getNode(config.bridge);
        if (!bridge) { return; }

        const close$ = new Subject();

        this.on('input', () => {
        });

        this.on('close', () => {
            close$.next();
            close$.complete();
        });
    }

    RED.nodes.registerType('rfm-multithermostat', MultiThermostatNode);
};
