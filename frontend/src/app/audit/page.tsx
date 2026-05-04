"use client";

import { DashboardLayout } from "@/components/layouts/DashboardLayout";
import { PageContainer } from "@/components/shared/PageContainer";
import { PageHeader } from "@/components/shared/PageHeader";
import { GlassCard, CardContent, CardHeader, CardTitle, CardDescription } from "@/components/shared/GlassCard";
import { FileText } from "lucide-react";

export default function AuditPage() {
  return (
    <DashboardLayout>
      <PageContainer>
        <PageHeader 
          title="Audit Log" 
          description="Append-only cryptographic record of parameter changes and system events."
          icon={FileText}
        />

        <GlassCard>
          <CardHeader>
            <CardTitle className="font-heading text-lg font-semibold tracking-tight text-gradient">System Events</CardTitle>
            <CardDescription className="text-white/40 text-[13px] leading-relaxed">
              A table of authenticated system modifications will appear here.
            </CardDescription>
          </CardHeader>
          <CardContent>
            <div className="flex flex-col items-center justify-center p-16 border border-white/5 border-dashed rounded-2xl text-white/40 bg-white/[0.02]">
              <FileText className="h-12 w-12 mb-4 opacity-20" />
              <p className="text-[13px] tracking-wide">No audit events recorded yet.</p>
            </div>
          </CardContent>
        </GlassCard>
      </PageContainer>
    </DashboardLayout>
  );
}
