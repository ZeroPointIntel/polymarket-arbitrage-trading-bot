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
    <div className="flex h-screen bg-background">
      {/* Sidebar */}
      <div className="w-64 border-r bg-card flex flex-col">
        <div className="flex h-16 items-center px-6 border-b">
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
                className={`flex items-center gap-3 px-3 py-2 rounded-md text-sm font-medium transition-colors ${
                  isActive 
                    ? "bg-primary text-primary-foreground" 
                    : "text-muted-foreground hover:bg-muted hover:text-foreground"
                }`}
              >
                <Icon className="h-4 w-4" />
                {item.name}
              </Link>
            );
          })}
        </nav>
        <div className="p-4 border-t">
          <button
            onClick={() => signOut({ callbackUrl: "/login" })}
            className="flex w-full items-center gap-3 px-3 py-2 rounded-md text-sm font-medium text-muted-foreground hover:bg-muted hover:text-foreground transition-colors"
          >
            <LogOut className="h-4 w-4" />
            Sign Out
          </button>
        </div>
      </div>

      {/* Main content */}
      <div className="flex-1 flex flex-col overflow-hidden">
        <header className="h-16 border-b flex items-center px-8 bg-card/50 backdrop-blur-sm z-10">
          <h1 className="text-lg font-semibold">
            {navigation.find((item) => item.href === pathname)?.name || "Dashboard"}
          </h1>
        </header>
        <main className="flex-1 overflow-y-auto p-8 bg-black/40">
          {children}
        </main>
      </div>
    </div>
  );
}
