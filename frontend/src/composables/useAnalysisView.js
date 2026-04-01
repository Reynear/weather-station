import { computed, ref } from 'vue'
import { storeToRefs } from 'pinia'
import { getLocalTimeZone } from '@internationalized/date'
import { metricMap, metricOrder } from '../lib/analysisMetrics'
import {
  convertMetricValue,
  formatMetricValue,
  resolveMetricUnit,
  resolveMetricUnitKey,
  resolveMetricUnitOptions,
  toNumber,
} from '../lib/metricFormatting'
import { useAppStore } from '../stores/appStore'
import { useMqttStore } from '../stores/mqttstore'

const EMPTY_AGGREGATE = {
  count: 0,
  min: null,
  max: null,
  avg: null,
  latest: null,
}

const chartTheme = {
  mode: 'dark',
  palette: 'palette1',
  monochrome: { enabled: false },
}

const metricTabs = metricOrder.map((metric) => ({
  label: metricMap[metric].label,
  value: metric,
}))

const dateFormatter = new Intl.DateTimeFormat(undefined, { dateStyle: 'medium' })

const formatCalendarLabel = (value) => (
  value ? dateFormatter.format(value.toDate(getLocalTimeZone())) : 'Select date'
)

const normalizeRows = (entries) => {
  return entries
    .map((entry) => {
      const timestamp = new Date(entry.timestamp).getTime()
      if (!Number.isFinite(timestamp)) return null

      return {
        timestamp,
        temperature: toNumber(entry.temperature),
        humidity: toNumber(entry.humidity),
        pressure: toNumber(entry.pressure),
        soil_moisture: toNumber(entry.soil_moisture),
        altitude: toNumber(entry.altitude),
        heat_index: toNumber(entry.heat_index),
      }
    })
    .filter(Boolean)
}

export const useAnalysisView = () => {
  const appStore = useAppStore()
  const mqttStore = useMqttStore()
  const {
    analysisRows,
    analysisAggregates,
    analysisLoading,
    analysisError,
    analysisFetched,
  } = storeToRefs(appStore)
  const { metricUnits } = storeToRefs(mqttStore)

  const startDate = ref()
  const endDate = ref()
  const activeMetricTab = ref(metricOrder[0])

  const rows = computed(() => normalizeRows(analysisRows.value))
  const loading = analysisLoading
  const error = analysisError
  const fetched = analysisFetched

  const startDateString = computed(() => startDate.value?.toString() ?? '')
  const endDateString = computed(() => endDate.value?.toString() ?? '')
  const startDateLabel = computed(() => formatCalendarLabel(startDate.value))
  const endDateLabel = computed(() => formatCalendarLabel(endDate.value))
  const isMultiDayRange = computed(() => (
    Boolean(startDateString.value && endDateString.value && startDateString.value !== endDateString.value)
  ))

  const selectedRangeLabel = computed(() => {
    if (!startDate.value || !endDate.value) return 'No range selected'
    return `${startDateLabel.value} to ${endDateLabel.value}`
  })

  const totalReadingCountLabel = computed(() => {
    const count = rows.value.length
    return count === 1 ? '1 row' : `${count} rows`
  })

  const activeMetric = computed(() => metricMap[activeMetricTab.value] ?? metricMap.temperature)
  const activeUnitOptions = computed(() => resolveMetricUnitOptions(activeMetricTab.value))
  const activeUnitKey = computed(() => resolveMetricUnitKey(activeMetricTab.value, metricUnits.value))
  const activeUnit = computed(() => resolveMetricUnit(activeMetricTab.value, metricUnits.value))
  const activeAggregate = computed(() => analysisAggregates.value[activeMetricTab.value] ?? EMPTY_AGGREGATE)
  const hasMetricData = computed(() => activeAggregate.value.count > 0)

  const formatDateTimeLabel = (timestamp) => {
    if (!Number.isFinite(timestamp)) return ''

    const date = new Date(timestamp)
    return isMultiDayRange.value
      ? date.toLocaleString([], {
          month: 'short',
          day: 'numeric',
          hour: 'numeric',
          minute: '2-digit',
        })
      : date.toLocaleTimeString([], {
          hour: 'numeric',
          minute: '2-digit',
        })
  }

  const formatDateTimeTooltip = (timestamp) => {
    if (!Number.isFinite(timestamp)) return 'No time'

    return new Date(timestamp).toLocaleString([], {
      year: 'numeric',
      month: 'short',
      day: 'numeric',
      hour: 'numeric',
      minute: '2-digit',
      second: '2-digit',
    })
  }

  const convertAggregatePoint = (point) => {
    if (!point) return null

    const timestamp = new Date(point.timestamp).getTime()
    return {
      value: convertMetricValue(activeMetricTab.value, metricUnits.value, point.value),
      timestamp: Number.isFinite(timestamp) ? timestamp : null,
    }
  }

  const statCards = computed(() => {
    const aggregate = activeAggregate.value
    const minPoint = convertAggregatePoint(aggregate.min)
    const maxPoint = convertAggregatePoint(aggregate.max)
    const latestPoint = convertAggregatePoint(aggregate.latest)
    const averageValue = convertMetricValue(activeMetricTab.value, metricUnits.value, aggregate.avg)

    return [
      {
        label: 'Min',
        value: minPoint ? formatMetricValue(minPoint.value, activeUnit.value) : 'N/A',
        meta: minPoint ? formatDateTimeTooltip(minPoint.timestamp) : 'No data in range',
      },
      {
        label: 'Max',
        value: maxPoint ? formatMetricValue(maxPoint.value, activeUnit.value) : 'N/A',
        meta: maxPoint ? formatDateTimeTooltip(maxPoint.timestamp) : 'No data in range',
      },
      {
        label: 'Avg',
        value: Number.isFinite(averageValue)
          ? formatMetricValue(averageValue, activeUnit.value)
          : 'N/A',
        meta: aggregate.count ? `From ${aggregate.count} readings` : 'No data in range',
      },
      {
        label: 'Latest',
        value: latestPoint ? formatMetricValue(latestPoint.value, activeUnit.value) : 'N/A',
        meta: latestPoint ? formatDateTimeTooltip(latestPoint.timestamp) : 'No data in range',
      },
    ]
  })

  const activeSummary = computed(() => {
    if (!activeAggregate.value.count) return 'No valid readings in this range.'

    const minPoint = convertAggregatePoint(activeAggregate.value.min)
    const maxPoint = convertAggregatePoint(activeAggregate.value.max)
    const latestPoint = convertAggregatePoint(activeAggregate.value.latest)
    const averageValue = convertMetricValue(activeMetricTab.value, metricUnits.value, activeAggregate.value.avg)

    return [
      `Min ${formatMetricValue(minPoint?.value, activeUnit.value)}`,
      `max ${formatMetricValue(maxPoint?.value, activeUnit.value)}`,
      `avg ${formatMetricValue(averageValue, activeUnit.value)}`,
      `latest ${formatMetricValue(latestPoint?.value, activeUnit.value)}.`,
    ].join(', ')
  })

  const activeChartSeries = computed(() => {
    return [
      {
        name: activeMetric.value.label,
        data: rows.value.flatMap((row) => {
          const value = row[activeMetricTab.value]
          if (value == null) return []

          return [{
            x: row.timestamp,
            y: convertMetricValue(activeMetricTab.value, metricUnits.value, value),
          }]
        }),
      },
    ]
  })

  const activeChartOptions = computed(() => {
    return {
      chart: {
        type: 'line',
        background: 'transparent',
        zoom: {
          enabled: true,
          type: 'x',
          autoScaleYaxis: true,
        },
        toolbar: {
          show: true,
          tools: {
            download: false,
            selection: false,
            zoom: true,
            zoomin: true,
            zoomout: true,
            pan: true,
            reset: true,
          },
          autoSelected: 'pan',
        },
      },
      theme: chartTheme,
      stroke: { curve: 'smooth', width: 2 },
      colors: [activeMetric.value.color],
      xaxis: {
        type: 'datetime',
        title: {
          text: isMultiDayRange.value ? 'Date and Time' : 'Time of Day',
          style: { color: '#94a3b8' },
        },
        labels: {
          style: { colors: '#94a3b8' },
          datetimeUTC: false,
          hideOverlappingLabels: true,
          formatter(value, timestamp) {
            return formatDateTimeLabel(timestamp)
          },
        },
        tickAmount: isMultiDayRange.value ? 8 : 6,
      },
      yaxis: {
        title: { text: activeUnit.value.title, style: { color: '#94a3b8' } },
        labels: {
          style: { colors: '#94a3b8' },
          formatter(value) {
            return formatMetricValue(value, activeUnit.value)
          },
        },
      },
      grid: { borderColor: '#1e293b' },
      tooltip: {
        theme: 'dark',
        x: {
          formatter(value) {
            return formatDateTimeTooltip(value)
          },
        },
        y: {
          formatter(value) {
            return formatMetricValue(value, activeUnit.value)
          },
        },
      },
      noData: { text: `No ${activeMetric.value.label.toLowerCase()} data` },
    }
  })

  const setActiveUnit = (unit) => {
    mqttStore.setMetricUnit(activeMetricTab.value, unit)
  }

  const loadSummary = async () => {
    await appStore.loadAnalysisSummary(startDate.value, endDate.value)
    if (analysisFetched.value) {
      activeMetricTab.value = metricOrder[0]
    }
  }

  return {
    metricTabs,
    startDate,
    endDate,
    startDateLabel,
    endDateLabel,
    loading,
    error,
    fetched,
    selectedRangeLabel,
    totalReadingCountLabel,
    activeMetricTab,
    activeMetric,
    activeUnitOptions,
    activeUnitKey,
    activeUnit,
    activeAggregate,
    hasMetricData,
    statCards,
    activeSummary,
    activeChartSeries,
    activeChartOptions,
    setActiveUnit,
    loadSummary,
  }
}
