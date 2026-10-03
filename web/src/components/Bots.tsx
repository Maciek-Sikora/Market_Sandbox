import { fmtSigned, type BotStat } from "../derive";

export function Bots({ stats, order }: { stats: Map<string, BotStat>; order: string[] }) {
  const rows = order.map((id) => stats.get(id)).filter((s): s is BotStat => !!s);
  const human = stats.get("human");
  if (human) rows.push(human);

  return (
    <div className="bots">
      <table>
        <thead>
          <tr>
            <th scope="col">Bot</th>
            <th scope="col" className="r">Orders</th>
            <th scope="col" className="r">Cancels</th>
            <th scope="col" className="r">Trades</th>
            <th scope="col" className="r">Position</th>
            <th scope="col" className="r">Profit and loss</th>
          </tr>
        </thead>
        <tbody>
          {rows.map((r) => (
            <tr key={r.id}>
              <th scope="row" className="num">{r.id === "human" ? "you" : r.id}</th>
              <td className="r num">{r.orders}</td>
              <td className="r num">{r.cancels}</td>
              <td className="r num">{r.fills}</td>
              <td className="r num">{r.position}</td>
              <td className={"r num " + (r.pnl > 0.005 ? "bid-t" : r.pnl < -0.005 ? "ask-t" : "")}>{fmtSigned(r.pnl)}</td>
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}
