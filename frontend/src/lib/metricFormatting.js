import { metricMap } from './analysisMetrics'

export const toNumber = (value) => {
  if (typeof value === 'number') return Number.isFinite(value) ? value : null
  if (typeof value === 'string' && value.trim() !== '') {
    const parsed = Number(value)
    return Number.isFinite(parsed) ? parsed : null
  }
  return null
}

export const resolveMetricUnitOptions = (metric) => {
  return Object.values(metricMap[metric]?.units ?? {})
}

export const resolveMetricUnitKey = (metric, metricUnits) => {
  const options = resolveMetricUnitOptions(metric)
  const selected = metricUnits?.[metric]
  return metricMap[metric]?.units?.[selected] ? selected : options[0]?.value
}

export const resolveMetricUnit = (metric, metricUnits) => {
  const unitKey = resolveMetricUnitKey(metric, metricUnits)
  return metricMap[metric]?.units?.[unitKey] ?? resolveMetricUnitOptions(metric)[0]
}

export const convertMetricValue = (metric, metricUnits, value) => {
  const unit = resolveMetricUnit(metric, metricUnits)
  return unit ? unit.convert(toNumber(value)) : toNumber(value)
}

export const formatMetricValue = (value, unit, fallback = 'N/A') => {
  if (!Number.isFinite(value)) return fallback

  const unitLabel = typeof unit === 'string' ? unit : unit?.label ?? ''
  const decimals = typeof unit === 'object' && unit !== null ? unit.decimals ?? 1 : 1
  const formatted = value.toFixed(decimals)

  if (!unitLabel) return formatted

  const separator = unitLabel.startsWith('°') || unitLabel === '%' ? '' : ' '
  return `${formatted}${separator}${unitLabel}`
}
