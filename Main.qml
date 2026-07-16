import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15



ApplicationWindow{


id:root


width:1280


height:720



visible:true



title:"GCS 地面控制站"



color:"#1e1e2e"







StackView{


id:stackView



anchors.fill:parent



initialItem:loginPage



}







Component{


id:loginPage




LoginPage{



onLoginSuccess:function(username){

historyDatabase.startSession(username)

stackView.push(mainPage)



}




}



}








Component{


id:mainPage



MainPage{


onOpenStm32Requested:{


stackView.push(stm32Page)


}

onOpenHistoryRequested:{

stackView.push(historyPage)

}



}



}





Component{

id:historyPage

HistoryPage{
onBackRequested: stackView.pop()

}

}

Component{


id:stm32Page



Stm32Page{
onBackRequested:{


stackView.pop()


}


}


}



}
