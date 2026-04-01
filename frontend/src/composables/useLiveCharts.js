import { computed } from 'vue'
import { storeToRefs } from 'pinia'
import { metricMap } from '../lib/analysisMetrics'
import { liveChartGroups } from '../lib/liveChartGroups'
import {
  convertMetricValue,
  formatMetricValue,
  resolveMetricUnit,
  resolveMetricUnitKey,
  resolveMetricUnitOptions,
} from '../lib/metricFormatting'
import { useMqttStore } from '../stores/mqttstore'

export const useLiveCharts = () => {
  const mqttStore = useMqttStore()
  const { history, metricUnits } = storeToRefs(mqttStore)

  const liveRange = computed(() => {
    const timestamps = Object.values(history.value)
      .flatMap((series) => series.map((point) => point.x))
      .filter(Number.isFinite)
      .sort((left, right) => left - right)

    if (!timestamps.length) return null

    return {
      min: timestamps[0],
      max: timestamps[timestamps.length - 1],
    }
  })

  const buildChartOptions = (colors, seriesKeys, range) => {
    const yAxisTitle = seriesKeys
      .map((metric) => {
        const unit = resolveMetricUnit(metric, metricUnits.value)
        return `${metricMap[metric].label} (${unit.label})`
      })
      .join(' / ')

    return {
      chart: {
        type: 'line',
        background: 'transparent',
        toolbar: { show: false },
        animations: { easing: 'linear' },
      },
      stroke: { curve: 'smooth', width: 2 },
      colors,
      xaxis: {
        type: 'datetime',
        title: { text: 'Recent Time', style: { color: '#94a3b8' } },
        labels: {
          style: { colors: '#94a3b8' },
          datetimeUTC: false,
          formatter(value, timestamp) {
            return new Date(timestamp).toLocaleTimeString([], {
              hour: 'numeric',
              minute: '2-digit',
              second: '2-digit',
            })
          },
        },
        tickAmount: 6,
        ...(range ? { min: range.min, max: range.max } : {}),
      },
      yaxis: {
        title: { text: yAxisTitle, style: { color: '#94a3b8' } },
        labels: { style: { colors: '#94a3b8' } },
      },
      grid: { borderColor: '#1e293b' },
      tooltip: {
        theme: 'dark',
        y: {
          formatter(value, { seriesIndex, w }) {
            const metric = seriesKeys[seriesIndex]
            const unit = resolveMetricUnit(metric, metricUnits.value)
            const label = w?.config?.series?.[seriesIndex]?.name
            const formatted = formatMetricValue(value, unit, '')
            return label ? `${label}: ${formatted}` : formatted
          },
        },
      },
      noData: { text: 'Waiting for live readings...' },
      legend: { labels: { colors: '#94a3b8' } },
    }
  }

  const charts = computed(() => {
    return liveChartGroups.map((group) => {
      const toggleGroups = group.series
        .map(({ key, name }) => {
          const options = resolveMetricUnitOptions(key)
          if (options.length <= 1) return null

          return {
            metric: key,
            label: name,
            options,
            unitKey: resolveMetricUnitKey(key, metricUnits.value),
          }
        })
        .filter(Boolean)

      return {
        ...group,
        toggleGroups,
        options: buildChartOptions(
          group.colors,
          group.series.map((item) => item.key),
          liveRange.value
        ),
        series: group.series.map((item) => ({
          name: item.name,
          data: history.value[item.key].map((point) => ({
            x: point.x,
            y: convertMetricValue(item.key, metricUnits.value, point.y),
          })),
        })),
      }
    })
  })

  const setUnit = (metric, unit) => {
    mqttStore.setMetricUnit(metric, unit)
  }

  return {
    charts,
    setUnit,
  }
}
