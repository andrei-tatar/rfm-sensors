const GAMMA = 2.8;

const MIN = 9e-3; //sec
const MAX = .1e-3; //sec
const TIMER = 2e6; //Hz

const brightness = new Array(100).fill(0).map((_, i) => i + 1);
const ticksMin = TIMER * MIN;
const ticksMax = TIMER * MAX;

const ticksArray = brightness.map(b => {
    const power = Math.sin(b / 100.0 * Math.PI / 2);
    const corrected = Math.pow(power, GAMMA);
    const ticks = (ticksMax - ticksMin) * corrected + ticksMin;
    return Math.round(ticks);
});

console.log(ticksArray.join(','));
