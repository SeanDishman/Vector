void fresh_checks() {
    check(has(script("function fail(e){throw Error(\"number\"==typeof e?\"[MobX] minified error nr: \"+e:\"[MobX] \"+e)}var admin=Symbol(\"mobx administration\");"),"MobX"),"MobX with the comparison written number first");
    check(has(script("function fail(e){throw new Error(typeof e===\"number\"?\"[MobX] minified error nr: \"+e:\"[MobX] \"+e)}var admin=Symbol(\"mobx administration\");"),"MobX"),"MobX with the comparison written typeof first");
    check(!has(script("function fail(e){throw Error(\"number\"==typeof e?\"[MobX] minified error nr: \"+e:e)}"),"MobX"),"one MobX marker insufficient");

    const std::string built="function load(key){let s=document.createElement(\"script\");s.src=`https://www.google.com/recaptcha/enterprise.js?render=${key}`;s.async=!0;document.body.appendChild(s)}";
    check(has(script(built),"reCAPTCHA"),"reCAPTCHA script built in page code");
    check(has(script("var n=document.createElement(\"script\");n.src=\"https://www.recaptcha.net/recaptcha/api.js?onload=ready&render=explicit\";"),"reCAPTCHA"),"reCAPTCHA loader on recaptcha.net");
    check(!has(script("const allowed=[{matcher:\"//www.google.com/recaptcha/api.js\",purposes:[\"Essential\"]},{matcher:\"https://www.google.com/recaptcha/enterprise.js?render=x\"}];"),"reCAPTCHA"),"reCAPTCHA addresses in an allow-list are not a loader");
    check(!has(script("const html='<script src=\"https://www.google.com/recaptcha/api.js?render=explicit\"></script>';"),"reCAPTCHA"),"reCAPTCHA markup in a string is not a built script");
    check(!has(script("/* s.src=\"https://www.google.com/recaptcha/api.js?render=explicit\" */var a=1;"),"reCAPTCHA"),"commented reCAPTCHA loader ignored");
    check(!has(script("var s=document.createElement(\"script\");s.src=\"https://www.google.com/recaptcha/api.js\";"),"reCAPTCHA"),"reCAPTCHA address without loader parameters insufficient");
    check(!has(script("var s=document.createElement(\"script\");s.src=\"https://www.google.com.other.example/recaptcha/api.js?render=explicit\";"),"reCAPTCHA"),"a lookalike reCAPTCHA host is not the loader");
    {
        std::vector<Finding> banner;
        identify_javascript("https://unseen.example/opaque","/*!\n * jQuery JavaScript Library v3.5.1\n * https://jquery.com/\n *\n * Includes Sizzle.js\n */\n(function(){})();",banner);
        check(std::any_of(banner.begin(),banner.end(),[](const auto& x){return x.name=="jQuery"&&x.version=="3.5.1";}),"jQuery uncompressed build banner with version");
        check(!has(script("var note=\"requires the jQuery JavaScript Library v3 or later\";"),"jQuery"),"prose naming jQuery without a release is not its banner");
    }
    {
        std::vector<Finding> found,single,commented;
        identify_stylesheet("https://unseen.example/a.css",":root,:host{--fa-family-brands:\"Font Awesome 7 Brands\";--fa-font-brands:normal 400 1em/1 var(--fa-family-brands)}@font-face{font-family:\"Font Awesome 7 Brands\";font-style:normal;font-weight:400;src:url(a.woff2)}",found);
        check(std::any_of(found.begin(),found.end(),[](const auto& x){return x.name=="Font Awesome"&&x.version=="7";}),"Font Awesome stylesheet variable with its font face");
        identify_stylesheet("https://unseen.example/b.css",".note:after{content:\"Font Awesome 7 Brands\"}:root{--fa-family-brands:\"Font Awesome 7 Brands\"}",single);
        check(!has(single,"Font Awesome"),"Font Awesome variable without its font face insufficient");
        identify_stylesheet("https://unseen.example/c.css","/* :root{--fa-family-brands:\"Font Awesome 7 Brands\"}@font-face{font-family:\"Font Awesome 7 Brands\"} */body{color:red}",commented);
        check(!has(commented,"Font Awesome"),"commented Font Awesome rules ignored");
    }
    {
        HttpObservation page;page.url="https://unseen.example/";page.status=200;
        page.body="<!doctype html><svg class=\"lucide lucide-panel-right inline\" viewBox=\"0 0 24 24\"></svg><svg class=\"h-4 lucide lucide-search\"></svg>";
        std::vector<Finding> icons,weak;
        identify_frontend_markup(page,icons);
        check(has(icons,"Lucide"),"Lucide icon classes on svg elements");
        page.body="<!doctype html><svg class=\"lucide lucide-search\"></svg><p class=\"lucide lucide-x\">lucide lucide-menu</p><svg class=\"lucide\"></svg><svg class=\"lucide-home\"></svg><svg class=\"lucide lucide-Bad_Name\"></svg>";
        identify_frontend_markup(page,weak);
        check(!has(weak,"Lucide"),"one Lucide icon, other elements and partial classes insufficient");
    }
    check(vendor(request("https://images.contentstack.io/v3/assets/blt0123456789abcdef/blt1/hero.png"),"Contentstack"),"Contentstack asset host");
    check(!vendor(request("https://images.contentstack.io.other.example/v3/assets/blt0/a.png"),"Contentstack"),"a lookalike Contentstack host is not the provider");
    check(!vendor(request("https://images.contentstack.io/status"),"Contentstack"),"a Contentstack path outside its asset route is not attributed");
    check(vendor(request("https://transcend-cdn.com/cm/0a1b2c3d-0000-4000-8000-000000000000/airgap.js"),"Transcend"),"Transcend consent script");
    check(!vendor(request("https://transcend-cdn.com/cm/0a1b2c3d/other.js"),"Transcend"),"another file on the Transcend host is not the consent script");
    check(!vendor(request("https://nottranscend-cdn.com/cm/0a1b2c3d/airgap.js"),"Transcend"),"a lookalike Transcend host is not the provider");
}
