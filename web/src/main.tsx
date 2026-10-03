import { StrictMode } from "react";
import { createRoot } from "react-dom/client";
import App from "./App";
import { feed } from "./feed";
import "./styles.css";

feed.start();

createRoot(document.getElementById("root")!).render(
  <StrictMode>
    <App />
  </StrictMode>,
);
