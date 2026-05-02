import { useState, useEffect } from "react";

export interface DHOpportunity {
  question: string;
  asset: string;
  yesPrice: number;
  noPrice: number;
  combined: number;
  discount: number;
  discountPct: number;
  endDate: string;
}

export interface LiveState {
  balance: number;
  winRate: number;
  openPositions: number;
  status: number;
  btcPrice: number;
  ethPrice: number;
  solPrice: number;
  fairValue: number;
  polymarketPrice: number;
  timestamp: number;
  marketsScanned: number;
  dhOpportunities: DHOpportunity[];
}

const defaultState: LiveState = {
  balance: 0,
  winRate: 0,
  openPositions: 0,
  status: 0,
  btcPrice: 0,
  ethPrice: 0,
  solPrice: 0,
  fairValue: 0,
  polymarketPrice: 0,
  timestamp: 0,
  marketsScanned: 0,
  dhOpportunities: [],
};

export function useLiveState() {
  const [state, setState] = useState<LiveState>(defaultState);

  useEffect(() => {
    const eventSource = new EventSource("/api/live");
    
    eventSource.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        setState(data);
      } catch (err) {
        console.error("Failed to parse live state:", err);
      }
    };

    return () => {
      eventSource.close();
    };
  }, []);

  return state;
}
