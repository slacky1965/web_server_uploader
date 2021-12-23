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
            document.getElementById("listing").innerHTML = data;
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

