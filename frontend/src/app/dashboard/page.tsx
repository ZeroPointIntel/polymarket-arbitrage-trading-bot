"use client";

import { DashboardLayout } from "@/components/layouts/DashboardLayout";
import { useLiveState } from "@/hooks/useLiveState";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { Activity, DollarSign, Briefcase, Percent } from "lucide-react";
import { useEffect, useRef } from "react";
import { createChart, IChartApi, ISeriesApi, LineSeries } from "lightweight-charts";

export default function DashboardPage() {
  const liveState = useLiveState();
  const chartContainerRef = useRef<HTMLDivElement>(null);
  const chartRef = useRef<IChartApi | null>(null);
  const lineSeriesRef = useRef<ISeriesApi<"Line"> | null>(null);
  const fairValueSeriesRef = useRef<ISeriesApi<"Line"> | null>(null);

  useEffect(() => {
    if (!chartContainerRef.current) return;

    const chart = createChart(chartContainerRef.current, {
      layout: {
        background: { color: "transparent" },
        textColor: "#d1d5db",
      },
      grid: {
        vertLines: { color: "#374151" },
        horzLines: { color: "#374151" },
      },
      width: chartContainerRef.current.clientWidth,
      height: 300,
    });

    const lineSeries = chart.addSeries(LineSeries, { color: "#3b82f6", lineWidth: 2, title: "BTC Price" });
    const fairValueSeries = chart.addSeries(LineSeries, { color: "#10b981", lineWidth: 2, title: "Fair Value (0-1)" });

    chartRef.current = chart;
    lineSeriesRef.current = lineSeries;
    fairValueSeriesRef.current = fairValueSeries;

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
    if (liveState.timestamp && lineSeriesRef.current && fairValueSeriesRef.current) {
      const time = (liveState.timestamp / 1000) as any;
      lineSeriesRef.current.update({ time, value: liveState.btcPrice });
      fairValueSeriesRef.current.update({ time, value: liveState.fairValue * 100000 }); // Scaled for visibility alongside BTC
    }
  }, [liveState]);

  return (
    <DashboardLayout>
      <div className="space-y-6">
        {/* KPI Metrics */}
        <div className="grid gap-4 md:grid-cols-2 lg:grid-cols-4">
          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Total Balance</CardTitle>
              <DollarSign className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">${liveState.balance.toFixed(2)}</div>
              <p className="text-xs text-muted-foreground">
                +20.1% from last month
              </p>
            </CardContent>
          </Card>
          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Open Positions</CardTitle>
              <Briefcase className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{liveState.openPositions}</div>
              <p className="text-xs text-muted-foreground">
                Active Polymarket contracts
              </p>
            </CardContent>
          </Card>
          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Win Rate</CardTitle>
              <Percent className="h-4 w-4 text-muted-foreground" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold">{(liveState.winRate * 100).toFixed(1)}%</div>
              <p className="text-xs text-muted-foreground">
                Last 30 days
              </p>
            </CardContent>
          </Card>
          <Card>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-sm font-medium">Core Status</CardTitle>
              <Activity className="h-4 w-4 text-green-500" />
            </CardHeader>
            <CardContent>
              <div className="text-2xl font-bold text-green-500">
                {liveState.status === 0 ? "OK" : "ERROR"}
              </div>
              <p className="text-xs text-muted-foreground">
                Last tick: {new Date(liveState.timestamp || Date.now()).toLocaleTimeString()}
              </p>
            </CardContent>
          </Card>
        </div>

        {/* Charts */}
        <Card className="col-span-4">
          <CardHeader>
            <CardTitle>Live Market Pricing (Mocked SSE)</CardTitle>
          </CardHeader>
          <CardContent>
            <div ref={chartContainerRef} className="w-full h-[300px]" />
          </CardContent>
        </Card>
      </div>
    </DashboardLayout>
  );
}
