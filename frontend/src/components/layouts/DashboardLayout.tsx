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
        <div className="flex h-14 items-center px-6 border-b border-white/10">
          <span className="font-heading text-lg font-extrabold tracking-tighter text-gradient-accent">
            TRADEBOT_PRO
          </span>
        </div>
        <nav className="flex-1 space-y-1 p-4 overflow-y-auto">
          {navigation.map((item) => {
            const Icon = item.icon;
            const isActive = pathname === item.href;
            return (
              <Link
                key={item.name}
                href={item.href}
                className={`flex items-center gap-3 px-3 py-2 rounded-xl text-[11px] font-bold tracking-[0.08em] uppercase transition-all ${
                  isActive 
                    ? "bg-white/15 text-white shadow-[0_4px_20px_rgba(0,0,0,0.3)] ring-1 ring-white/10" 
                    : "text-white/40 hover:bg-white/5 hover:text-white/80"
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
            className="flex w-full items-center gap-3 px-3 py-2 rounded-xl text-[11px] font-bold tracking-[0.08em] uppercase text-white/40 hover:bg-white/5 hover:text-white/80 transition-all"
          >
            <LogOut className="h-4 w-4" />
            Sign Out
          </button>
        </div>
      </div>

      {/* Main content */}
      <div className="flex-1 flex flex-col overflow-hidden relative z-10">
        <header className="h-14 flex items-center px-8 sticky top-0 z-30 glass border-t-0 border-x-0 rounded-none">
          <h1 className="text-[10px] font-bold tracking-widest uppercase text-white/30">
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
