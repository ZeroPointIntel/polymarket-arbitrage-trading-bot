export const dynamic = "force-dynamic";

export async function GET(req: Request) {
  let counter = 0;
  
  const stream = new ReadableStream({
    start(controller) {
      const encoder = new TextEncoder();
      
      const pushState = () => {
        counter++;
        
        // Mock data that fluctuates
        const baseBalance = 1000.0;
        const volatility = (Math.random() - 0.5) * 5;
        const balance = baseBalance + volatility;
        
        // Mock BTC price around 98000
        const btcPrice = 98000 + (Math.random() - 0.5) * 50;
        const fairValue = btcPrice > 98000 ? 0.6 : 0.4;
        
        const state = {
          balance,
          winRate: 0.0,
          openPositions: counter % 10 === 0 ? 1 : 0,
          status: 0,
          btcPrice,
          fairValue,
          timestamp: Date.now(),
        };

        const data = `data: ${JSON.stringify(state)}\n\n`;
        controller.enqueue(encoder.encode(data));
      };

      // Push initial state immediately
      pushState();

      // Push updates every 50ms to simulate the trading core tick
      const interval = setInterval(pushState, 50);

      req.signal.addEventListener("abort", () => {
        clearInterval(interval);
        controller.close();
      });
    },
  });

  return new Response(stream, {
    headers: {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache, no-transform",
      "Connection": "keep-alive",
    },
  });
}
