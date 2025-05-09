import { Outlet } from "react-router-dom";

// TODO latter add a protectoed routes and userProveder
function App() {
    return (
        <div className="App">
            <Outlet />
        </div>
    );
}
export default App;