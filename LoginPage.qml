import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 2.15


Rectangle {


    id:root


    color:"#1a1a2e"



    signal loginSuccess(string username)





    property bool isLoginMode:true





    ColumnLayout{


        anchors.centerIn:parent


        width:360


        spacing:20





        Text{


            text:"GCS 地面控制站"


            color:"#c9a84c"


            font.pixelSize:32


            font.bold:true



            Layout.alignment:
            Qt.AlignHCenter


        }






        Text{


            text:
            isLoginMode?
            "用户登录":
            "用户注册"



            color:"#8a9ba8"



            font.pixelSize:18



            Layout.alignment:
            Qt.AlignHCenter



        }








        TextField{


            id:userInput



            Layout.fillWidth:true



            height:45



            placeholderText:"用户名"



            color:"white"



            font.pixelSize:16




            background:Rectangle{


                color:"#2a2a3e"


                radius:6



                border.color:

                userInput.activeFocus?

                "#c9a84c":

                "#444444"



            }



        }









        TextField{


            id:passwordInput



            Layout.fillWidth:true



            height:45



            placeholderText:"密码"



            echoMode:
            TextField.Password



            color:"white"



            font.pixelSize:16




            background:Rectangle{


                color:"#2a2a3e"


                radius:6



                border.color:

                passwordInput.activeFocus?

                "#c9a84c":

                "#444444"



            }




        }









        TextField{


            id:confirmInput



            Layout.fillWidth:true



            height:45



            visible:
            !isLoginMode



            placeholderText:
            "确认密码"



            echoMode:
            TextField.Password



            color:"white"





            background:Rectangle{


                color:"#2a2a3e"


                radius:6



                border.color:"#444444"



            }



        }









        Button{


            Layout.fillWidth:true


            height:48



            text:

            isLoginMode?

            "登 录":

            "注 册"





            background:Rectangle{


                color:"#c9a84c"


                radius:6



            }





            contentItem:Text{


                text:parent.text



                color:"#1a1a2e"



                font.pixelSize:18



                font.bold:true



                horizontalAlignment:
                Text.AlignHCenter



                verticalAlignment:
                Text.AlignVCenter



            }






            onClicked:{



                if(isLoginMode)

                {

                    login()

                }

                else

                {

                    registerUser()

                }



            }




        }









        Button{


            Layout.fillWidth:true



            text:


            isLoginMode?

            "没有账号？注册":

            "已有账号？登录"





            background:Rectangle{


                color:"transparent"


            }





            contentItem:Text{


                text:parent.text



                color:"#8a9ba8"



                horizontalAlignment:
                Text.AlignHCenter



            }







            onClicked:{


                isLoginMode=!isLoginMode



                userInput.clear()

                passwordInput.clear()

                confirmInput.clear()



            }




        }





    }








    //==================
    // 登录
    //==================


    function login()

    {


        var user=
        userInput.text.trim()



        var pwd=
        passwordInput.text.trim()





        if(user==="admin"
                &&
           pwd==="123456")

        {


            console.log(
            "登录成功"
            )



            root.loginSuccess(user)



        }

        else

        {


            console.log(
            "用户名或密码错误"
            )


        }



    }






    //==================
    // 注册
    //==================


    function registerUser()

    {


        var user=
        userInput.text.trim()



        var pwd=
        passwordInput.text.trim()



        var confirm=
        confirmInput.text.trim()






        if(user===""
           ||
           pwd===""
           ||
           confirm==="")

        {


            console.log(
            "请输入完整信息"
            )


            return

        }






        if(pwd!==confirm)

        {


            console.log(
            "密码不一致"
            )


            return


        }






        console.log(
        "注册成功"
        )



        isLoginMode=true



        userInput.text=user



        passwordInput.clear()



        confirmInput.clear()



    }



}
