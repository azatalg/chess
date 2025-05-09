import { Children, lazy } from "react";
import App from "../App";

const Home = lazy(() => import("../pages/Home"));
const GameWithBot = lazy(() => import("../pages/GameWithBot"));
const GameWithPlayer = lazy(() => import("../pages/GameWithPlayer"));

const Routes = [
    {
        path: "/",
        element: <App/>,
        children: [
            {
                index: true,
                element: <Home/>
            },
            {
                path: "game/bot",
                element: <GameWithBot/> 
            },
            { 
                path: "game/player",
                element: <GameWithPlayer/>
            }
        ]
    }
]

export default Routes;