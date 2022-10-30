from flask import Flask,request
#render_template, request, flash, get_flashed_messages
from flask import render_template #与html交互
web = Flask(__name__)

@web.route('/',methods=["POST","GET"])            #主页
def hello_world():
    if request.method == "GET":
        return render_template("index.html")
    
web.run(port=5000,debug=True)