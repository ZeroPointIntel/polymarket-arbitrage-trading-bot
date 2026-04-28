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
            <CardTitle>Recent Trades</CardTitle>
            <CardDescription>
              A table of the latest closed positions will appear here once connected to the Database/Core.
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="flex flex-col items-center justify-center p-12 border border-white/5 border-dashed rounded-lg text-muted-foreground bg-black/20">
              <History className="h-12 w-12 mb-4 opacity-30" />
              <p>No trades executed yet in this session.</p>
            </div>
          </CardContent>
        </GlassCard>
      </PageContainer>
    </DashboardLayout>
  );
}
