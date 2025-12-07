<!DOCTYPE html>
<html lang="zh-CN">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>你好PHP - 我的第一个PHP网页</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
            font-family: 'Segoe UI', 'Microsoft YaHei', sans-serif;
        }
        
        body {
            background-color: #f5f7fa;
            color: #333;
            line-height: 1.6;
            padding: 20px;
        }
        
        .container {
            max-width: 1000px;
            margin: 0 auto;
            background-color: white;
            border-radius: 12px;
            box-shadow: 0 5px 25px rgba(0, 0, 0, 0.08);
            overflow: hidden;
        }
        
        header {
            background: linear-gradient(135deg, #4a6fa5 0%, #2c3e50 100%);
            color: white;
            padding: 40px 30px;
            text-align: center;
        }
        
        h1 {
            font-size: 2.8rem;
            margin-bottom: 10px;
            letter-spacing: 1px;
        }
        
        .subtitle {
            font-size: 1.2rem;
            opacity: 0.9;
            font-weight: 300;
        }
        
        .content {
            padding: 30px;
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 30px;
        }
        
        @media (max-width: 768px) {
            .content {
                grid-template-columns: 1fr;
            }
        }
        
        .card {
            background-color: #fff;
            border-radius: 10px;
            padding: 25px;
            box-shadow: 0 3px 15px rgba(0, 0, 0, 0.05);
            transition: transform 0.3s ease;
        }
        
        .card:hover {
            transform: translateY(-5px);
        }
        
        .card h2 {
            color: #2c3e50;
            border-bottom: 2px solid #4a6fa5;
            padding-bottom: 10px;
            margin-bottom: 20px;
            font-size: 1.6rem;
        }
        
        .php-output {
            background-color: #f8f9fa;
            border-left: 4px solid #4a6fa5;
            padding: 15px;
            margin: 15px 0;
            border-radius: 0 5px 5px 0;
            overflow-x: auto;
        }
        
        .highlight {
            color: #e74c3c;
            font-weight: 600;
        }
        
        .info-list {
            list-style-type: none;
        }
        
        .info-list li {
            padding: 8px 0;
            border-bottom: 1px dashed #eee;
        }
        
        .info-list li:last-child {
            border-bottom: none;
        }
        
        .form-group {
            margin-bottom: 20px;
        }
        
        label {
            display: block;
            margin-bottom: 8px;
            font-weight: 600;
            color: #2c3e50;
        }
        
        input[type="text"] {
            width: 100%;
            padding: 12px 15px;
            border: 1px solid #ddd;
            border-radius: 6px;
            font-size: 1rem;
            transition: border 0.3s;
        }
        
        input[type="text"]:focus {
            border-color: #4a6fa5;
            outline: none;
        }
        
        button {
            background-color: #4a6fa5;
            color: white;
            border: none;
            padding: 12px 25px;
            border-radius: 6px;
            cursor: pointer;
            font-size: 1rem;
            font-weight: 600;
            transition: background-color 0.3s;
        }
        
        button:hover {
            background-color: #3a5a80;
        }
        
        .footer {
            text-align: center;
            padding: 25px;
            background-color: #f8f9fa;
            color: #666;
            border-top: 1px solid #eee;
            font-size: 0.95rem;
        }
        
        .server-info {
            background-color: #2c3e50;
            color: white;
            padding: 15px;
            border-radius: 6px;
            margin-top: 20px;
            font-size: 0.9rem;
        }
        
        .greeting-box {
            background: linear-gradient(to right, #4a6fa5, #6a8bc5);
            color: white;
            padding: 20px;
            border-radius: 8px;
            text-align: center;
            font-size: 1.5rem;
            margin: 20px 0;
        }
    </style>
</head>
<body>
    <div class="container">
        <header>
            <h1>你好，PHP！</h1>
            <p class="subtitle">我的第一个PHP动态网页 | <?php echo date('Y年m月d日'); ?></p>
        </header>
        
        <div class="content">
            <!-- PHP基础信息展示 -->
            <div class="card">
                <h2>PHP基本信息</h2>
                <div class="php-output">
                    <?php
                    // 基本的PHP信息
                    echo "<p><strong>PHP版本：</strong>" . phpversion() . "</p>";
                    echo "<p><strong>服务器软件：</strong>" . $_SERVER['SERVER_SOFTWARE'] . "</p>";
                    echo "<p><strong>运行方式：</strong>" . php_sapi_name() . "</p>";
                    
                    // 获取当前时间
                    date_default_timezone_set('Asia/Shanghai');
                    echo "<p><strong>当前时间：</strong>" . date('H时i分s秒') . "</p>";
                    ?>
                </div>
                
                <h3>PHP功能演示</h3>
                <ul class="info-list">
                    <?php
                    // 使用PHP变量和数组
                    $features = ["动态内容生成", "表单处理", "数据库连接", "会话管理", "文件操作"];
                    
                    foreach($features as $index => $feature) {
                        echo "<li>功能 " . ($index+1) . ": <span class='highlight'>$feature</span></li>";
                    }
                    ?>
                </ul>
            </div>
            
            <!-- PHP交互演示 -->
            <div class="card">
                <h2>PHP交互演示</h2>
                
                <!-- 问候表单 -->
                <form method="POST" action="">
                    <div class="form-group">
                        <label for="name">请输入您的名字：</label>
                        <input type="text" id="name" name="name" placeholder="例如：张三" value="<?php echo isset($_POST['name']) ? htmlspecialchars($_POST['name']) : ''; ?>">
                    </div>
                    
                    <button type="submit" name="submit">提交问候</button>
                </form>
                
                <?php
                // 处理表单提交
                if(isset($_POST['submit']) && !empty($_POST['name'])) {
                    $name = htmlspecialchars($_POST['name']);
                    $hour = date('H');
                    
                    // 根据时间选择不同的问候语
                    if($hour < 6) {
                        $greeting = "深夜好";
                    } elseif($hour < 12) {
                        $greeting = "早上好";
                    } elseif($hour < 18) {
                        $greeting = "下午好";
                    } else {
                        $greeting = "晚上好";
                    }
                    
                    echo "<div class='greeting-box'>";
                    echo "$greeting，<strong>$name</strong>！欢迎来到PHP世界！";
                    echo "</div>";
                    
                    // 显示名字的长度
                    $nameLength = mb_strlen($name, 'UTF-8');
                    echo "<p class='php-output'>您的名字 <span class='highlight'>$name</span> 有 $nameLength 个字符。</p>";
                } elseif(isset($_POST['submit'])) {
                    echo "<p class='php-output' style='color:#e74c3c;'>请先输入您的名字。</p>";
                }
                ?>
                
                <h3>服务器环境</h3>
                <div class="server-info">
                    <?php
                    echo "<p><strong>服务器IP：</strong>" . $_SERVER['SERVER_ADDR'] . "</p>";
                    echo "<p><strong>客户端IP：</strong>" . $_SERVER['REMOTE_ADDR'] . "</p>";
                    echo "<p><strong>请求方法：</strong>" . $_SERVER['REQUEST_METHOD'] . "</p>";
                    ?>
                </div>
            </div>
        </div>
        
        <div class="footer">
            <p>本页面使用 <span class="highlight">PHP <?php echo phpversion(); ?></span> 动态生成 | 最后更新: <?php echo date('Y-m-d H:i:s'); ?></p>
            <p>PHP是一种流行的服务器端脚本语言，特别适合Web开发。</p>
        </div>
    </div>
    
    <script>
        // 简单的客户端交互
        document.addEventListener('DOMContentLoaded', function() {
            // 为输入框添加焦点效果
            const nameInput = document.getElementById('name');
            if(nameInput) {
                nameInput.addEventListener('focus', function() {
                    this.style.boxShadow = '0 0 0 3px rgba(74, 111, 165, 0.2)';
                });
                
                nameInput.addEventListener('blur', function() {
                    this.style.boxShadow = 'none';
                });
            }
            
            // 显示页面加载完成时间
            console.log('页面加载完成时间: ' + new Date().toLocaleTimeString());
        });
    </script>
</body>
</html>