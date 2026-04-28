"use client";

import Link from "next/link";
import { usePathname } from "next/navigation";
import { Activity, ShieldAlert, SlidersHorizontal, History, FileText, LogOut } from "lucide-react";
import { signOut } from "next-auth/react";

const navigation = [
  { name: "Dashboard", href: "/dashboard", icon: Activity },
  { name: "Strategies", href: "/strategies", icon: SlidersHorizontal },
  { name: "Risk Limits", href: "/risk", icon: ShieldAlert },
  { name: "Trade History", href: "/history", icon: History },
  { name: "Audit Log", href: "/audit", icon: FileText },
];

export function DashboardLayout({ children }: { children: React.ReactNode }) {
  const pathname = usePathname();

  return (
    <div className="flex h-screen text-white selection:bg-white/20 relative">
      <div className="mesh-bg" />
      {/* Sidebar */}
      <div className="w-64 glass flex flex-col z-20 relative border-r-0 border-y-0">
        <div className="flex h-16 items-center px-6 border-b border-white/10">
          <span className="text-xl font-bold tracking-tight">TradeBot Pro</span>
        </div>
        <nav className="flex-1 space-y-1 p-4 overflow-y-auto">
          {navigation.map((item) => {
            const Icon = item.icon;
            const isActive = pathname === item.href;
            return (
              <Link
                key={item.name}
                href={item.href}
                className={`flex items-center gap-3 px-3 py-2 rounded-xl text-sm font-medium transition-all ${
                  isActive 
                    ? "bg-white/20 text-white shadow-[0_4px_12px_rgba(0,0,0,0.1)]" 
                    : "text-white/60 hover:bg-white/10 hover:text-white"
                }`}
              >
                <Icon className="h-4 w-4" />
                {item.name}
              </Link>
            );
          })}
        </nav>
        <div className="p-4 border-t border-white/10">
          <button
            onClick={() => signOut({ callbackUrl: "/login" })}
            className="flex w-full items-center gap-3 px-3 py-2 rounded-xl text-sm font-medium text-white/60 hover:bg-white/10 hover:text-white transition-all"
          >
            <LogOut className="h-4 w-4" />
            Sign Out
          </button>
        </div>
      </div>

      {/* Main content */}
      <div className="flex-1 flex flex-col overflow-hidden relative z-10">
        <header className="h-16 flex items-center px-8 sticky top-0 z-30 glass border-t-0 border-x-0 rounded-none">
          <h1 className="text-lg font-semibold text-white">
            {navigation.find((item) => item.href === pathname)?.name || "Dashboard"}
          </h1>
        </header>
        <main className="flex-1 overflow-y-auto p-8 relative">
          <div className="relative z-10">
            {children}
          </div>
        </main>
      </div>
    </div>
  );
}
