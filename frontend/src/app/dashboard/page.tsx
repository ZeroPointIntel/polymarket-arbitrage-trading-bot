"use client";

import { DashboardLayout } from "@/components/layouts/DashboardLayout";
import { PageContainer } from "@/components/shared/PageContainer";
import { PageHeader } from "@/components/shared/PageHeader";
import { GlassCard, CardContent, CardHeader, CardTitle } from "@/components/shared/GlassCard";
import { useLiveState } from "@/hooks/useLiveState";
import { Activity, DollarSign, Briefcase, Percent } from "lucide-react";
import { useEffect, useRef, useState } from "react";
import { createChart, IChartApi, ISeriesApi, LineSeries } from "lightweight-charts";

export default function DashboardPage() {
  const liveState = useLiveState();
  const [mounted, setMounted] = useState(false);
  
  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const lineSeriesRef = useRef<ISeriesApi<"Line"> | null>(null);
  const fairValueSeriesRef = useRef<ISeriesApi<"Line"> | null>(null);
  const pmSeriesRef = useRef<ISeriesApi<"Line"> | null>(null);

  useEffect(() => {
    setMounted(true);
    if (!chartContainerRef.current) return;

    const chart = createChart(chartContainerRef.current, {
      layout: {
        background: { color: "transparent" },
        textColor: "#9ca3af",
      },
      grid: {
        vertLines: { color: "rgba(255, 255, 255, 0.05)" },
        horzLines: { color: "rgba(255, 255, 255, 0.05)" },
      },
      rightPriceScale: {
        visible: true,
      },
      leftPriceScale: {
        visible: true,
      },
      width: chartContainerRef.current.clientWidth,
      height: 300,
    });

    const lineSeries = chart.addSeries(LineSeries, { color: "#38bdf8", lineWidth: 2, title: "BTC Price" });
    const fairValueSeries = chart.addSeries(LineSeries, { 
      color: "#34d399", 
      lineWidth: 2, 
      title: "Fair Value (0-1)",
      priceScaleId: "left"
    });
    const pmSeries = chart.addSeries(LineSeries, { 
      color: "#f472b6", 
      lineWidth: 2, 
      title: "Polymarket YES Price",
      priceScaleId: "left"
    });

    chartRef.current = chart;
    lineSeriesRef.current = lineSeries;
    fairValueSeriesRef.current = fairValueSeries;
    pmSeriesRef.current = pmSeries;

    const handleResize = () => {
      if (chartContainerRef.current) {
        chart.applyOptions({ width: chartContainerRef.current.clientWidth });
      }
    };
    window.addEventListener("resize", handleResize);

    return () => {
      window.removeEventListener("resize", handleResize);
      chart.remove();
    };
  }, []);

  useEffect(() => {
    if (liveState.timestamp && lineSeriesRef.current && fairValueSeriesRef.current && pmSeriesRef.current) {
      const time = (liveState.timestamp / 1000) as any;
      lineSeriesRef.current.update({ time, value: liveState.btcPrice });
      fairValueSeriesRef.current.update({ time, value: liveState.fairValue });
      pmSeriesRef.current.update({ time, value: liveState.polymarketPrice });
    }
  }, [liveState]);

  return (
    <DashboardLayout>
      <PageContainer>
        <PageHeader 
          title="Dashboard" 
          description="Real-time control plane and live state metrics from the C++ trading core." 
          icon={Activity} 
        />
        
        {/* KPI Metrics */}
        <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-4">
          <GlassCard>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium text-primary">Total Balance</CardTitle>
              <DollarSign className="h-4 w-4 text-primary opacity-80" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">${liveState.balance.toFixed(2)}</div>
              <p className="text-xs text-muted-foreground mt-1">
                +20.1% from last month
              </p>
            </CardContent>
          </GlassCard>

          <GlassCard>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium text-primary">Open Positions</CardTitle>
              <Briefcase className="h-4 w-4 text-primary opacity-80" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{liveState.openPositions}</div>
              <p className="text-xs text-muted-foreground mt-1">
                Active Polymarket contracts
              </p>
            </CardContent>
          </GlassCard>

          <GlassCard>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium text-primary">Win Rate</CardTitle>
              <Percent className="h-4 w-4 text-primary opacity-80" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{(liveState.winRate * 100).toFixed(1)}%</div>
              <p className="text-xs text-muted-foreground mt-1">
                Last 30 days
              </p>
            </CardContent>
          </GlassCard>

          <GlassCard>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium text-primary">Core Status</CardTitle>
              <Activity className="h-4 w-4 text-emerald-400 opacity-80" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold text-emerald-400">
                {liveState.status === 0 ? "OK" : "ERROR"}
              </div>
              <p className="text-xs text-muted-foreground mt-1">
                {/* Defer date rendering to client-side to fix hydration mismatch */}
                Last tick: {mounted ? new Date(liveState.timestamp || Date.now()).toLocaleTimeString() : "--:--:--"}
              </p>
            </CardContent>
          </GlassCard>
        </div>

        {/* Charts */}
        <GlassCard>
          <CardHeader>
            <CardTitle>Live Market Pricing (SSE)</CardTitle>
          </CardHeader>
          <CardContent>
            <div ref={chartContainerRef} className="w-full h-[300px]" />
          </CardContent>
        </GlassCard>
      </PageContainer>
    </DashboardLayout>
  );
}
