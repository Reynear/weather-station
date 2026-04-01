const roundValue = (value, decimals = 1) => {
  if (!Number.isFinite(value)) return null

  const factor = 10 ** decimals
  return Math.round((value + Number.EPSILON) * factor) / factor
}

const convertTemperature = (value, unit) => {
  if (!Number.isFinite(value)) return null

  return unit === 'fahrenheit'
    ? roundValue((value * 9) / 5 + 32, 1)
    : roundValue(value, 1)
}

export const metricOrder = [
  'temperature',
  'humidity',
  'pressure',
  'soil_moisture',
  'altitude',
  'heat_index',
]

export const metricMap = {
  temperature: {
    label: 'Temperature',
    description: 'Air temperature in this range.',
    color: '#38bdf8',
    units: {
      celsius: {
        value: 'celsius',
        label: '°C',
        toggleLabel: 'Celsius',
        title: 'Temperature (°C)',
        decimals: 1,
        convert: (value) => convertTemperature(value, 'celsius'),
      },
      fahrenheit: {
        value: 'fahrenheit',
        label: '°F',
        toggleLabel: 'Fahrenheit',
        title: 'Temperature (°F)',
        decimals: 1,
        convert: (value) => convertTemperature(value, 'fahrenheit'),
      },
    },
  },
  humidity: {
    label: 'Humidity',
    description: 'Humidity in this range.',
    color: '#0ea5e9',
    units: {
      percent: {
        value: 'percent',
        label: '%',
        toggleLabel: 'Percent',
        title: 'Humidity (%)',
        decimals: 1,
        convert: (value) => roundValue(value, 1),
      },
    },
  },
  pressure: {
    label: 'Pressure',
    description: 'Pressure in this range.',
    color: '#8b5cf6',
    units: {
      hpa: {
        value: 'hpa',
        label: 'hPa',
        toggleLabel: 'hPa',
        title: 'Pressure (hPa)',
        decimals: 1,
        convert: (value) => roundValue(value, 1),
      },
      mmhg: {
        value: 'mmhg',
        label: 'mmHg',
        toggleLabel: 'mmHg',
        title: 'Pressure (mmHg)',
        decimals: 1,
        convert: (value) => roundValue(value * 0.750061683, 1),
      },
    },
  },
  soil_moisture: {
    label: 'Soil',
    description: 'Soil readings in this range.',
    color: '#10b981',
    units: {
      percent: {
        value: 'percent',
        label: '%',
        toggleLabel: 'Percent',
        title: 'Soil (%)',
        decimals: 1,
        convert: (value) => roundValue(value, 1),
      },
    },
  },
  altitude: {
    label: 'Altitude',
    description: 'Altitude in this range.',
    color: '#ec4899',
    units: {
      meters: {
        value: 'meters',
        label: 'm',
        toggleLabel: 'Meters',
        title: 'Altitude (m)',
        decimals: 1,
        convert: (value) => roundValue(value, 1),
      },
      feet: {
        value: 'feet',
        label: 'ft',
        toggleLabel: 'Feet',
        title: 'Altitude (ft)',
        decimals: 1,
        convert: (value) => roundValue(value * 3.280839895, 1),
      },
    },
  },
  heat_index: {
    label: 'Heat Index',
    description: 'Heat index in this range.',
    color: '#f97316',
    units: {
      celsius: {
        value: 'celsius',
        label: '°C',
        toggleLabel: 'Celsius',
        title: 'Heat Index (°C)',
        decimals: 1,
        convert: (value) => convertTemperature(value, 'celsius'),
      },
      fahrenheit: {
        value: 'fahrenheit',
        label: '°F',
        toggleLabel: 'Fahrenheit',
        title: 'Heat Index (°F)',
        decimals: 1,
        convert: (value) => convertTemperature(value, 'fahrenheit'),
      },
    },
  },
}

export const defaultMetricUnits = Object.fromEntries(
  metricOrder.map((metric) => [metric, Object.keys(metricMap[metric].units)[0]])
)
