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
  const [maxPosition, setMaxPosition] = useState([8]); // 8%
  const [dailyLoss, setDailyLoss] = useState([20]); // 20%
  const [drawdownKill, setDrawdownKill] = useState([40]); // 40%
  const [loading, setLoading] = useState(false);

  const handleSave = async () => {
    setLoading(true);
    // In reality, this would prompt for WebAuthn/Re-auth before saving to the DB/Core
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
            <CardTitle className="text-primary">Max Position Fraction</CardTitle>
            <CardDescription>
              The maximum percentage of the total portfolio balance that can be allocated to a single trade.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex justify-between items-center mb-2">
              <Label className="text-white font-medium">Value: {maxPosition[0]}%</Label>
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
            <CardTitle className="text-primary">Daily Loss Limit</CardTitle>
            <CardDescription>
              If the daily trailing PnL hits this percentage, all trading halts for 24 hours.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex justify-between items-center mb-2">
              <Label className="text-white font-medium">Value: -{dailyLoss[0]}%</Label>
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

        <GlassCard className="border-destructive/30">
          <CardHeader>
            <CardTitle className="text-destructive">Total Drawdown Kill Switch</CardTitle>
            <CardDescription>
              If total portfolio value drops below this percentage of the initial balance, the bot liquidates all positions and halts indefinitely.
            </CardDescription>
          </CardHeader>
          <CardContent className="space-y-4">
            <div className="flex justify-between items-center mb-2">
              <Label className="text-destructive font-bold">Value: -{drawdownKill[0]}%</Label>
            </div>
            <Slider 
              value={drawdownKill} 
              onValueChange={(val) => setDrawdownKill(val as number[])} 
              max={100} 
              step={1} 
              className="[&_[role=slider]]:bg-destructive [&_[role=slider]]:border-destructive py-4"
            />
          </CardContent>
          <CardFooter className="bg-destructive/10 border-t border-destructive/20 py-3 mt-4">
            <p className="text-xs text-destructive/80 font-medium flex items-center gap-2">
              <ShieldAlert className="h-4 w-4" />
              Warning: Modifying this value requires a fresh security challenge.
            </p>
          </CardFooter>
        </GlassCard>

        <div className="flex justify-end pt-4">
          <Button onClick={handleSave} disabled={loading} size="lg" className="shadow-[0_0_15px_rgba(0,255,255,0.2)]">
            {loading ? "Verifying & Saving..." : "Save Changes"}
          </Button>
        </div>
      </PageContainer>
    </DashboardLayout>
  );
}
