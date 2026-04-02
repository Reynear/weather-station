<template>
  <div class="flex flex-col gap-6">
    <UCard v-for="chart in charts" :key="chart.title">
      <template #header>
        <div class="flex flex-col gap-4">
          <div>
            <h3 class="text-lg font-semibold">{{ chart.title }}</h3>
            <p>{{ chart.description }}</p>
          </div>

          <div class="flex flex-wrap gap-4" v-if="chart.toggleGroups.length">
            <div v-for="toggle in chart.toggleGroups" :key="toggle.metric" class="flex flex-col gap-2">
              <div class="text-sm text-slate-400">{{ toggle.label }}</div>
              <div class="inline-flex rounded-xl border border-slate-800 bg-slate-950/60 p-1">
                <UButton
                  v-for="option in toggle.options"
                  :key="option.value"
                  :color="toggle.unitKey === option.value ? 'primary' : 'neutral'"
                  :variant="toggle.unitKey === option.value ? 'solid' : 'ghost'"
                  size="sm"
                  @click="setUnit(toggle.metric, option.value)"
                >
                  {{ option.toggleLabel }}
                </UButton>
              </div>
            </div>
          </div>
        </div>
      </template>

      <apexchart
        type="line"
        height="320"
        :options="chart.options"
        :series="chart.series"
      />
    </UCard>
  </div>
</template>

<script setup>
import { useLiveCharts } from '../composables/useLiveCharts'
import { useLiveStream } from '../composables/useLiveStream'

useLiveStream()

const { charts, setUnit } = useLiveCharts()
</script>
