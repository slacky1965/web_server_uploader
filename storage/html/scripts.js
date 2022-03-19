function setFileName(elem) {
    var fileName = elem.files[0].name;
    if (elem.id == "newhtmlfile") {
        document.getElementById("uploadhtml").value = fileName;
    } else if (elem.id == "newbinfile") {
        document.getElementById("uploadbin").value = fileName;
    }
}

async function upload(elem) {
    var fileName = elem.value;
    var upload_path;
    var fileInput;
    var element;
    if (elem.id == "uploadhtml") {
        upload_path = "/upload/html/" + fileName;
        element = document.getElementById("newhtmlfile");
    } else if (elem.id == "uploadbin") {
        upload_path = "/upload/image/" + fileName;
        element = document.getElementById("newbinfile");
    }

    fileInput = element.files;

    if (fileInput.length == 0) {
        alert("No file selected!");
    } else {
        var file = fileInput[0];

        document.getElementById("uploading").innerHTML = "Uploading! Please wait.";
        
        document.getElementById("uploadbin").disabled = true;
        document.getElementById("uploadhtml").disabled = true;
        
        try {
            var response = await fetch(upload_path, {
                method: 'POST',
                body: file
            });
            if (response.ok) {
                var data = await response.text();
                alert(data);
            } else {
                var error = await response.text();
                var message = `${error}. HTTP error ${response.status}.`;
                alert(message);
            }
            
        }
        catch(error) {
            alert(`Error! ${error}`);
        }
        
        document.getElementById("newbinfile").value = "";
        document.getElementById("newhtmlfile").value = "";

        location.reload();
    }
}

async function listing() {
    
    var list_url = "list";
    
    try {
        var response = await fetch(list_url, {
            method: 'POST'
        });
        if (response.ok) {
            var data = await response.text();
            document.getElementById("listing").innerHTML = data + "\n<input type=\"button\" id=\"files_delete\" value=\"Delete\" onclick=\"files_delete()\">\n";
        } else {
            var error = await response.text();
            var message = `${error}. HTTP error ${response.status}.`;
            alert(message);
        }

    }
    catch (error) {
        alert(`Error! ${error}`);
    }

}

async function files_delete() {
    var del = false;
    var del_files = {"Files": []}; 
    var checkbox=document.getElementsByTagName('input');
    for (var i = 0; i < checkbox.length; i++) {
        if (checkbox[i].type == "checkbox") {
            if (checkbox[i].checked == true) {
                del_files.Files.push(checkbox[i].value);
                del = true;
            }
        }
    }
    
    if (del) {
        var delete_url = "delete";
        var json = JSON.stringify(del_files);
        var buff_len = 2048;
        if (json.length > buff_len) {
            let message = `The inline buffer is full (${json.length} > ${buff_len}), you need to reduce the number of deleted files`
            alert(message);
            return;
        }
    
        try {
            var response = await fetch(delete_url, {
                method: 'POST',
                headers: {
                    "Content-Type": "application/json; charset=utf-8"
                },
                body: json
            });
            if (response.ok) {
                var data = await response.text();
                alert(data);
                location.reload();
            } else {
                var error = await response.text();
                var message = `${error}. HTTP error ${response.status}.`;
                alert(message);
            }

        }
        catch (error) {
            alert(`Error! ${error}`);
        }

    } else {
        alert(`Select the files to be deleted`);        
    }
    
}

