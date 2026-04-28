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
  const ethSeriesRef = useRef<ISeriesApi<"Line"> | null>(null);
  const solSeriesRef = useRef<ISeriesApi<"Line"> | null>(null);
  const fairValueSeriesRef = useRef<ISeriesApi<"Line"> | null>(null);
  const pmSeriesRef = useRef<ISeriesApi<"Line"> | null>(null);

  useEffect(() => {
    setMounted(true);
    if (!chartContainerRef.current) return;

    const chart = createChart(chartContainerRef.current, {
      layout: {
        background: { color: "transparent" },
        textColor: "rgba(255,255,255,0.3)",
        fontFamily: "var(--font-jetbrains-mono)",
        fontSize: 10,
      },
      grid: {
        vertLines: { color: "rgba(255, 255, 255, 0.02)" },
        horzLines: { color: "rgba(255, 255, 255, 0.02)" },
      },
      rightPriceScale: {
        visible: true,
        borderColor: "rgba(255, 255, 255, 0.05)",
      },
      leftPriceScale: {
        visible: true,
        borderColor: "rgba(255, 255, 255, 0.05)",
      },
      timeScale: {
        borderColor: "rgba(255, 255, 255, 0.05)",
      },
      crosshair: {
        horzLine: { color: "rgba(255, 255, 255, 0.1)", style: 3 },
        vertLine: { color: "rgba(255, 255, 255, 0.1)", style: 3 },
      },
      width: chartContainerRef.current.clientWidth,
      height: 360,
    });

    const lineSeries = chart.addSeries(LineSeries, { color: "#818cf8", lineWidth: 2, title: "BTC" });
    const ethSeries = chart.addSeries(LineSeries, { color: "#6366f1", lineWidth: 2, title: "ETH" });
    const solSeries = chart.addSeries(LineSeries, { color: "#14b8a6", lineWidth: 2, title: "SOL" });
    
    const fairValueSeries = chart.addSeries(LineSeries, { 
      color: "#34d399", 
      lineWidth: 1, 
      lineStyle: 2,
      title: "Fair Value",
      priceScaleId: "left"
    });
    const pmSeries = chart.addSeries(LineSeries, { 
      color: "#f472b6", 
      lineWidth: 1, 
      lineStyle: 2,
      title: "PM YES",
      priceScaleId: "left"
    });

    chartRef.current = chart;
    lineSeriesRef.current = lineSeries;
    ethSeriesRef.current = ethSeries;
    solSeriesRef.current = solSeries;
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
    if (liveState.timestamp && lineSeriesRef.current && ethSeriesRef.current && solSeriesRef.current && fairValueSeriesRef.current && pmSeriesRef.current) {
      const time = (liveState.timestamp / 1000) as any;
      lineSeriesRef.current.update({ time, value: liveState.btcPrice });
      ethSeriesRef.current.update({ time, value: liveState.ethPrice });
      solSeriesRef.current.update({ time, value: liveState.solPrice });
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
        <div className="grid gap-5 md:grid-cols-2 lg:grid-cols-4">
          <GlassCard>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-[11px] font-medium tracking-widest uppercase text-white/40">Total Balance</CardTitle>
              <DollarSign className="h-4 w-4 text-white/20" />
            </CardHeader>
            <CardContent className="pb-4">
              <div className="text-2xl font-mono font-extrabold tracking-tighter text-white">
                ${liveState.balance.toFixed(2)}
              </div>
              <p className="text-xs text-emerald-400 font-mono mt-3 flex items-center gap-1.5">
                <span className="bg-emerald-400/10 px-1.5 py-0.5 rounded text-[10px]">UP 20.1%</span>
                <span className="text-white/30">vs last month</span>
              </p>
            </CardContent>
          </GlassCard>

          <GlassCard>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-[11px] font-medium tracking-widest uppercase text-white/40">Open Positions</CardTitle>
              <Briefcase className="h-4 w-4 text-white/20" />
            </CardHeader>
            <CardContent className="pb-4">
              <div className="text-2xl font-mono font-extrabold tracking-tighter text-white">
                {liveState.openPositions}
              </div>
              <p className="text-xs text-white/40 mt-3 font-medium tracking-tight">
                ACTIVE CONTRACTS
              </p>
            </CardContent>
          </GlassCard>

          <GlassCard>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-[11px] font-medium tracking-widest uppercase text-white/40">Win Rate</CardTitle>
              <Percent className="h-4 w-4 text-white/20" />
            </CardHeader>
            <CardContent className="pb-4">
              <div className="text-2xl font-mono font-extrabold tracking-tighter text-white">
                {(liveState.winRate * 100).toFixed(1)}%
              </div>
              <p className="text-xs text-white/40 mt-3 font-medium tracking-tight">
                HISTORICAL PERFORMANCE
              </p>
            </CardContent>
          </GlassCard>

          <GlassCard>
            <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
              <CardTitle className="text-[11px] font-medium tracking-widest uppercase text-white/40">Core Status</CardTitle>
              <Activity className="h-4 w-4 text-emerald-400/60" />
            </CardHeader>
            <CardContent className="pb-4">
              <div className="flex items-center gap-3">
                <span className="relative flex h-2 w-2">
                  <span className="animate-ping absolute inline-flex h-full w-full rounded-full bg-emerald-400 opacity-75"></span>
                  <span className="relative inline-flex rounded-full h-2 w-2 bg-emerald-400"></span>
                </span>
                <span className="text-2xl font-mono font-extrabold tracking-tighter text-emerald-400">
                  {liveState.status === 0 ? "STABLE" : "ERROR"}
                </span>
              </div>
              <p className="text-xs text-white/30 mt-3 font-mono">
                {/* Defer date rendering to client-side to fix hydration mismatch */}
                LAST_TICK: {mounted ? new Date(liveState.timestamp || Date.now()).toLocaleTimeString() : "--:--:--"}
              </p>
            </CardContent>
          </GlassCard>
        </div>

        {/* Charts */}
        <GlassCard>
          <CardHeader>
            <div className="flex items-center justify-between">
              <CardTitle className="font-heading text-lg font-semibold tracking-tight text-gradient">Live Market Pricing</CardTitle>
              <span className="text-[11px] font-mono text-white/30 tracking-wider uppercase">SSE · 50ms tick</span>
            </div>
          </CardHeader>
          <CardContent>
            <div ref={chartContainerRef} className="w-full h-[340px]" />
          </CardContent>
        </GlassCard>
        {/* DH Screener Table */}
        <GlassCard>
          <CardHeader>
            <div className="flex items-center justify-between">
              <CardTitle className="font-heading text-lg font-semibold tracking-tight text-gradient">Live Arbitrage Screener</CardTitle>
              <span className="text-[11px] font-mono text-white/30 tracking-wider uppercase">Scanning {liveState.marketsScanned || 0} markets</span>
            </div>
          </CardHeader>
          <CardContent>
            <div className="overflow-x-auto">
              <table className="w-full text-sm text-left">
                <thead className="text-[9px] text-white/20 uppercase font-mono border-b border-white/5">
                  <tr>
                    <th className="px-4 py-2 font-medium">Market</th>
                    <th className="px-4 py-2 font-medium text-right">YES</th>
                    <th className="px-4 py-2 font-medium text-right">NO</th>
                    <th className="px-4 py-2 font-medium text-right">Sum</th>
                    <th className="px-4 py-2 font-medium text-right text-emerald-400">Disc.</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-white/5">
                  {(liveState.dhOpportunities || []).map((opp, idx) => (
                    <tr key={idx} className="hover:bg-white/[0.02] transition-colors">
                      <td className="px-4 py-2 font-medium text-white/70 max-w-md truncate text-xs" title={opp.question}>
                        {opp.question}
                      </td>
                      <td className="px-4 py-2 text-right font-mono text-white/50 text-xs">
                        {opp.yesPrice.toFixed(3)}
                      </td>
                      <td className="px-4 py-2 text-right font-mono text-white/50 text-xs">
                        {opp.noPrice.toFixed(3)}
                      </td>
                      <td className="px-4 py-2 text-right font-mono text-white/50 text-xs">
                        {opp.combined.toFixed(3)}
                      </td>
                      <td className="px-4 py-2 text-right font-mono font-bold text-emerald-400 bg-emerald-400/5 text-xs">
                        {opp.discountPct.toFixed(2)}%
                      </td>
                    </tr>
                  ))}
                  {(!liveState.dhOpportunities || liveState.dhOpportunities.length === 0) && (
                    <tr>
                      <td colSpan={5} className="px-4 py-8 text-center text-white/30 text-xs font-mono">
                        No active discount opportunities found in the top {liveState.marketsScanned || 0} markets.
                      </td>
                    </tr>
                  )}
                </tbody>
              </table>
            </div>
          </CardContent>
        </GlassCard>
      </PageContainer>
    </DashboardLayout>
  );
}
