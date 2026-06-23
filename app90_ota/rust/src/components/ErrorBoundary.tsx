import { Component, type ReactNode } from "react";

interface Props {
    children: ReactNode;
}

interface State {
    hasError: boolean;
    error: string | null;
}

export class ErrorBoundary extends Component<Props, State> {
    constructor(props: Props) {
        super(props);
        this.state = { hasError: false, error: null };
    }

    static getDerivedStateFromError(error: Error) {
        return { hasError: true, error: error.message };
    }

    componentDidCatch(error: Error, info: { componentStack: string }) {
        console.error("[ErrorBoundary]", error, info);
    }

    render() {
        if (this.state.hasError) {
            return (
                <div style={{
                    display: "flex",
                    flexDirection: "column",
                    alignItems: "center",
                    justifyContent: "center",
                    height: "100vh",
                    background: "#1a1a2e",
                    color: "#ef5350",
                    fontFamily: "monospace",
                    padding: "40px",
                }}>
                    <h1 style={{ marginBottom: "16px" }}>⚠ 应用错误</h1>
                    <pre style={{
                        background: "#0a0f1a",
                        padding: "16px",
                        borderRadius: "6px",
                        maxWidth: "800px",
                        overflow: "auto",
                        whiteSpace: "pre-wrap",
                        color: "#e0e0e0",
                    }}>
                        {this.state.error || "Unknown error"}
                    </pre>
                </div>
            );
        }
        return this.props.children;
    }
}
