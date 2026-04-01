<template>
  <div class="space-y-6">
    <UCard>
      <template #header>
        <div class="space-y-2">
          <div class="flex items-center gap-3">
            <h2 class="text-2xl font-semibold">Live Readings</h2>
            <UBadge :color="statusBadge.color">
              {{ statusBadge.label }}
            </UBadge>
          </div>
        </div>
      </template>

      <UAlert
        v-if="statusAlert"
        :color="statusAlert.color"
        :title="statusAlert.title"
        :description="statusAlert.description"
      />

      <div class="grid gap-4 md:grid-cols-2 xl:grid-cols-3">
        <UCard v-for="item in liveMetrics" :key="item.key">
          <template #header>
            <div class="space-y-3">
              <div class="flex items-center justify-between gap-3">
                <span>{{ item.label }}</span>
                <span class="text-sm text-slate-400">{{ item.unit.label }}</span>
              </div>

              <div
                v-if="item.unitOptions.length > 1"
                class="inline-flex rounded-xl border border-slate-800 bg-slate-950/60 p-1"
              >
                <UButton
                  v-for="option in item.unitOptions"
                  :key="option.value"
                  :color="item.unitKey === option.value ? 'primary' : 'neutral'"
                  :variant="item.unitKey === option.value ? 'solid' : 'ghost'"
                  size="sm"
                  @click="setUnit(item.key, option.value)"
                >
                  {{ option.toggleLabel }}
                </UButton>
              </div>
            </div>
          </template>

          <div class="flex items-end justify-between gap-4">
            <div class="text-3xl font-semibold">{{ item.value }}</div>
            <div class="text-right">
              <div>{{ item.unit.label }}</div>
              <div class="mt-2 flex justify-end">
                <span class="rounded-full bg-slate-800 p-2">
                  <UIcon
                    :name="item.icon"
                    class="h-6 w-6"
                    :class="item.iconClass"
                  />
                </span>
              </div>
            </div>
          </div>
        </UCard>
      </div>
    </UCard>
  </div>
</template>

<script setup>
import { useLiveMetricCards } from '../composables/useLiveMetricCards'
import { useLiveStream } from '../composables/useLiveStream'

useLiveStream()

const { liveMetrics, statusAlert, statusBadge, setUnit } = useLiveMetricCards()
</script>
