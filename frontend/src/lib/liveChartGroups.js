export const liveChartGroups = [
  {
    title: 'Temperature, Humidity and Heat Index',
    description: 'Recent live trend.',
    colors: ['#fb7185', '#38bdf8', '#f97316'],
    series: [
      { key: 'temperature', name: 'Temperature' },
      { key: 'humidity', name: 'Humidity' },
      { key: 'heat_index', name: 'Heat Index' },
    ],
  },
  {
    title: 'Pressure, Soil and Altitude',
    description: 'Recent sensor trend.',
    colors: ['#8b5cf6', '#10b981', '#f59e0b'],
    series: [
      { key: 'pressure', name: 'Pressure' },
      { key: 'soil_moisture', name: 'Soil' },
      { key: 'altitude', name: 'Altitude' },
    ],
  },
]
