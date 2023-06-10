# KataCoffee
嘗試解決 [Coffee](https://boardgamegeek.com/boardgame/94746/coffee)(2011發行的桌遊)  
規則參考[nestorgames](https://www.nestorgames.com/rulebooks/COFFEE_EN.pdf)  
由於Python真的太慢了所以嘗試改寫KataGo的程式。  
現在目標是打敗[iggamecenter](https://www.iggamecenter.com/)上的機器人，因此棋盤大小設為5x5，四連線則勝利。（5,5,4）  
## Todo
chain是為了數氣，所以不需要。也不需要處理劫。  
- [ ] 修改成Coffee的規則  
- [ ] 把selfplay中貼目和讓子等圍棋特有的功能去掉  
## Note
計分和判斷棋局結束是在game/boardhistory中，但game/board有一些輔助的工具，我在修改時會把這些盡量都放在game/board中。  
Coffee的一個動作包含了方向和位置，因此在輸出位置或印出棋盤時都要加入方向。  
對稱：因為Coffee動作的侷限性，不會有對稱棋的問題。對稱僅用於神經網路的輸入、增加訓練資料用。  
getSituationRulesAndKoHash都改用board.pos_hash，因為沒有規則、打劫問題。  
KataGo有很多優化手段，目前GraphSearch、subtreeValueBias應該會保留，其他還需要研究一下。  
## Input
### V1:
SPATIAL:  
|channel|定義|
|---|---|
|1|是否在棋盤上|
|2|自己/對手的棋子|
|4|最後一手的位置，分四個方向|
|4|最後五手的位置（不含最後一手），不分方向（這個feature可能用處不大，但可以試試看每一手都給4個channel）|
|1|根據上一手的方向、位置，合法的位置|
|3|距離連線勝利差{1,2,3}步的棋子，不分顏色，比如約定四子獲勝則所有三子連線的位置在第一個channel會是1|
共15個channel  
GLOBAL:  
|channel|定義|
|---|---|
|1|獲勝所需的連線數|
目前沒想到其他的，而且我不認為連線數會有很大的用處。  
V1以後還有可能修改。  