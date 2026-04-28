"use client";

import { DashboardLayout } from "@/components/layouts/DashboardLayout";
import { PageContainer } from "@/components/shared/PageContainer";
import { PageHeader } from "@/components/shared/PageHeader";
import { GlassCard, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/shared/GlassCard";
import { History } from "lucide-react";

export default function HistoryPage() {
  return (
    <DashboardLayout>
      <PageContainer>
        <PageHeader 
          title="Trade History" 
          description="Historical record of all executed trades, PnL, and fees."
          icon={History}
        />

        <GlassCard>
          <CardHeader>
            <CardTitle className="font-heading text-lg font-semibold tracking-tight text-gradient">Recent Trades</CardTitle>
            <CardDescription className="text-white/40 text-[13px] leading-relaxed">
              A table of the latest closed positions will appear here once connected to the Database/Core.
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="flex flex-col items-center justify-center p-16 border border-white/5 border-dashed rounded-2xl text-white/40 bg-white/[0.02]">
              <History className="h-12 w-12 mb-4 opacity-20" />
              <p className="text-[13px] tracking-wide">No trades executed yet in this session.</p>
            </div>
          </CardContent>
        </GlassCard>
      </PageContainer>
    </DashboardLayout>
  );
}
