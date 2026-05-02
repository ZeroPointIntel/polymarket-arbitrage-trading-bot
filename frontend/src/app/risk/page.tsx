"use client";

import { DashboardLayout } from "@/components/layouts/DashboardLayout";
import { PageContainer } from "@/components/shared/PageContainer";
import { PageHeader } from "@/components/shared/PageHeader";
import { GlassCard, CardContent, CardHeader, CardTitle, CardDescription, CardFooter } from "@/components/shared/GlassCard";
import { Slider } from "@/components/ui/slider";
import { Label } from "@/components/ui/label";
import { Button } from "@/components/ui/button";
import { useState } from "react";
import { ShieldAlert } from "lucide-react";

export default function RiskPage() {
  const [maxPosition, setMaxPosition] = useState([8]);
  const [dailyLoss, setDailyLoss] = useState([20]);
  const [drawdownKill, setDrawdownKill] = useState([40]);
  const [loading, setLoading] = useState(false);

  const handleSave = async () => {
    setLoading(true);
    setTimeout(() => {
      setLoading(false);
      alert("Risk limits updated securely!");
    }, 1000);
  };

  return (
    <DashboardLayout>
      <PageContainer>
        <PageHeader 
          title="Risk Limits" 
          description="Control the automated safety thresholds. The RiskManager will immediately enforce these values across all active orders."
          icon={ShieldAlert}
        />

        <GlassCard>
          <CardHeader>
            <CardTitle className="font-heading text-lg font-semibold tracking-tight text-gradient">Max Position Fraction</CardTitle>
            <CardDescription className="text-white/40 text-[13px] leading-relaxed">
              The maximum percentage of the total portfolio balance that can be allocated to a single trade.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex justify-between items-center mb-2">
              <Label className="text-white/90 font-medium text-[14px]">Value</Label>
              <span className="font-mono text-2xl font-semibold text-white">{maxPosition[0]}%</span>
            </div>
            <Slider 
              value={maxPosition} 
              onValueChange={(val) => setMaxPosition(val as number[])} 
              max={50} 
              step={1} 
              className="py-4"
            />
          </CardContent>
        </GlassCard>

        <GlassCard>
          <CardHeader>
            <CardTitle className="font-heading text-lg font-semibold tracking-tight text-gradient">Daily Loss Limit</CardTitle>
            <CardDescription className="text-white/40 text-[13px] leading-relaxed">
              If the daily trailing PnL hits this percentage, all trading halts for 24 hours.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex justify-between items-center mb-2">
              <Label className="text-white/90 font-medium text-[14px]">Value</Label>
              <span className="font-mono text-2xl font-semibold text-amber-400">-{dailyLoss[0]}%</span>
            </div>
            <Slider 
              value={dailyLoss} 
              onValueChange={(val) => setDailyLoss(val as number[])} 
              max={100} 
              step={1} 
              className="py-4"
            />
          </CardContent>
        </GlassCard>

        <GlassCard className="border-red-500/20">
          <CardHeader>
            <CardTitle className="font-heading text-lg font-semibold tracking-tight text-red-400">Total Drawdown Kill Switch</CardTitle>
            <CardDescription className="text-white/40 text-[13px] leading-relaxed">
              If total portfolio value drops below this percentage of the initial balance, the bot liquidates all positions and halts indefinitely.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex justify-between items-center mb-2">
              <Label className="text-red-400/80 font-bold text-[14px]">Value</Label>
              <span className="font-mono text-2xl font-bold text-red-400">-{drawdownKill[0]}%</span>
            </div>
            <Slider 
              value={drawdownKill} 
              onValueChange={(val) => setDrawdownKill(val as number[])} 
              max={100} 
              step={1} 
              className="[&_[role=slider]]:bg-red-400 [&_[role=slider]]:border-red-400 py-4"
            />
          </CardContent>
          <CardFooter className="bg-red-500/5 border-t border-red-500/10 py-3 mt-4">
            <p className="text-[11px] text-red-400/60 font-medium flex items-center gap-2 tracking-wide">
              <ShieldAlert className="h-3.5 w-3.5" />
              Warning: Modifying this value requires a fresh security challenge.
            </p>
          </CardFooter>
        </GlassCard>

        <div className="flex justify-end pt-4">
          <Button 
            onClick={handleSave} 
            disabled={loading} 
            size="lg" 
            variant="glass"
            className="px-8 font-extrabold tracking-tight rounded-2xl"
          >
            {loading ? "INITIALIZING SECURE PUSH..." : "SAVE RISK LIMITS"}
          </Button>
        </div>
      </PageContainer>
    </DashboardLayout>
  );
}
